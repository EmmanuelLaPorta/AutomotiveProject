// SimpleSwitch.cc - VERSIONE MODULARE
#include "SimpleSwitch.h"
#include "../messages/EthernetFrame_m.h"
#include <sstream>

Define_Module(SimpleSwitch);

void SimpleSwitch::initialize() {
    datarate = par("datarate").doubleValue();
    switchingDelay = par("switchingDelay");
    
    int numPorts = gateSize("port");
    
    for (int i = 0; i < numPorts; i++) {
        portBusy[i] = false;
        maxQueueSize[i] = 0;
    }
    
    // ✅ SOLUZIONE MODULARE: Leggi la MAC table dal parametro NED
    loadMacTableFromParameter();
    
    EV << "=== SimpleSwitch Initialization ===" << endl;
    EV << "  Switch: " << getParentModule()->getFullName() << endl;
    EV << "  Numero porte: " << numPorts << endl;
    EV << "  Datarate: " << (datarate/1e9) << " Gbps" << endl;
    EV << "  Switching Delay: " << (switchingDelay.dbl()*1e6) << " us" << endl;
    EV << "  MAC Table Entries: " << macTable.size() << endl;
    
    // Debug: stampa la MAC table caricata
    if (!macTable.empty()) {
        EV << "=== MAC Table Caricata ===" << endl;
        for (const auto& entry : macTable) {
            EV << "    " << entry.first << " → porta " << entry.second << endl;
        }
    } else {
        EV_WARN << "ATTENZIONE: MAC Table vuota!" << endl;
    }
}

void SimpleSwitch::loadMacTableFromParameter() {
    // ✅ DEBUG: Verifica che il parametro esista
    EV << "=== Loading MAC Table ===" << endl;
    EV << "Switch path: " << getParentModule()->getFullPath() << endl;
    
    if (!hasPar("macTableConfig")) {
        EV_ERROR << "ERRORE: Parametro 'macTableConfig' NON TROVATO!" << endl;
        return;
    }
    
    // Leggi il parametro "macTableConfig"
    const char* configStr = par("macTableConfig").stringValue();
    
    EV << "Valore parametro: '" << configStr << "'" << endl;
    
    if (strlen(configStr) == 0) {
        EV_WARN << "Parametro macTableConfig VUOTO!" << endl;
        return;
    }
    
    EV << "Parsing MAC Table: " << configStr << endl;
    
    // ✅ NUOVO FORMATO: "MAC1->port1, MAC2->port2, MAC3->port3"
    // Usiamo "->" invece di ":" per evitare conflitti con i MAC address
    std::stringstream ss(configStr);
    std::string entry;
    
    while (std::getline(ss, entry, ',')) {
        // Rimuovi spazi
        entry.erase(0, entry.find_first_not_of(" \t"));
        entry.erase(entry.find_last_not_of(" \t") + 1);
        
        if (entry.empty()) continue;
        
        // Split su '->'
        size_t arrowPos = entry.find("->");
        if (arrowPos == std::string::npos) {
            EV_WARN << "Entry malformata (manca '->'): " << entry << endl;
            continue;
        }
        
        std::string mac = entry.substr(0, arrowPos);
        std::string portStr = entry.substr(arrowPos + 2);  // +2 per saltare "->"
        
        // Rimuovi spazi
        mac.erase(0, mac.find_first_not_of(" \t"));
        mac.erase(mac.find_last_not_of(" \t") + 1);
        portStr.erase(0, portStr.find_first_not_of(" \t"));
        portStr.erase(portStr.find_last_not_of(" \t") + 1);
        
        int port = std::stoi(portStr);
        
        macTable[mac] = port;
        EV << "  Configurato: " << mac << " → porta " << port << endl;
    }
    
    EV << "MAC Table caricata con " << macTable.size() << " entries" << endl;
}

void SimpleSwitch::handleMessage(cMessage *msg) {
    if (msg->isSelfMessage()) {
        if (strcmp(msg->getName(), "TxComplete") == 0) {
            int port = msg->getKind();
            delete msg;
            
            portBusy[port] = false;
            
            EV_DEBUG << "Trasmissione completata su porta " << port << endl;
            
            if (!portQueues[port].empty()) {
                transmitFromPort(port);
            }
        }
        else if (strcmp(msg->getName(), "ProcessFrame") == 0) {
            cPacket *frame = static_cast<cPacket*>(msg->getContextPointer());
            int arrivalPort = msg->getKind();
            
            delete msg;
            processFrame(frame, arrivalPort);
        }
        
        return;
    }
    
    // Frame in arrivo dalla rete
    cPacket *frame = check_and_cast<cPacket*>(msg);
    int arrivalPort = frame->getArrivalGate()->getIndex();
    
    EV_DEBUG << "Frame ricevuto su porta " << arrivalPort << endl;
    
    // Simula switching delay
    cMessage *processTimer = new cMessage("ProcessFrame");
    processTimer->setKind(arrivalPort);
    processTimer->setContextPointer(frame);
    
    scheduleAt(simTime() + switchingDelay, processTimer);
}

void SimpleSwitch::processFrame(cPacket *pkt, int arrivalPort) {
    EthernetFrame *frame = dynamic_cast<EthernetFrame*>(pkt);
    if (!frame) {
        EV_WARN << "Pacchetto non è un EthernetFrame, scartato" << endl;
        delete pkt;
        return;
    }
    
    std::string srcMac = frame->getSrc();
    std::string dstMac = frame->getDst();
    
    EV_DEBUG << "Processing: " << srcMac << " → " << dstMac << " (porta arrivo=" << arrivalPort << ")" << endl;
    
    // Determina porta di destinazione
    int destPort = lookupPort(dstMac);
    
    if (destPort == -1) {
        EV_WARN << "MAC " << dstMac << " SCONOSCIUTO, frame SCARTATO" << endl;
        delete frame;
        return;
    }
    
    if (destPort == arrivalPort) {
        EV_DEBUG << "Loop prevention: porta dest == porta arrivo, scartato" << endl;
        delete frame;
        return;
    }
    
    // Accoda
    portQueues[destPort].push(frame);
    
    int qSize = portQueues[destPort].size();
    if (qSize > maxQueueSize[destPort]) {
        maxQueueSize[destPort] = qSize;
    }
    
    EV_DEBUG << "Frame accodato su porta " << destPort << " (queue=" << qSize << ")" << endl;
    
    if (!portBusy[destPort]) {
        transmitFromPort(destPort);
    }
}

void SimpleSwitch::transmitFromPort(int port) {
    if (portQueues[port].empty()) {
        return;
    }
    
    cPacket *frame = portQueues[port].front();
    portQueues[port].pop();
    
    portBusy[port] = true;
    
    uint64_t bits = frame->getBitLength();
    simtime_t txTime = SimTime((double)bits / datarate, SIMTIME_S);
    
    EV_DEBUG << "TX porta " << port << " (" << bits << " bit, " << (txTime.dbl()*1e6) << " us)" << endl;
    
    send(frame, "port$o", port);
    
    cMessage *txComplete = new cMessage("TxComplete");
    txComplete->setKind(port);
    scheduleAt(simTime() + txTime, txComplete);
}

int SimpleSwitch::lookupPort(const std::string& dstMac) {
    auto it = macTable.find(dstMac);
    if (it != macTable.end()) {
        return it->second;
    }
    
    EV_DEBUG << "MAC " << dstMac << " non trovato in MAC table" << endl;
    return -1;
}

void SimpleSwitch::finish() {
    EV << "=== SimpleSwitch Statistics ===" << endl;
    
    for (const auto& pair : maxQueueSize) {
        int port = pair.first;
        int maxQ = pair.second;
        
        EV << "  Porta " << port << ": Max Queue = " << maxQ << " pacchetti" << endl;
        
        std::string statName = "port" + std::to_string(port) + "_maxQueue";
        recordScalar(statName.c_str(), maxQ);
    }
    
    EV << "=== Final MAC Table ===" << endl;
    for (const auto& entry : macTable) {
        EV << "  " << entry.first << " → porta " << entry.second << endl;
    }
}