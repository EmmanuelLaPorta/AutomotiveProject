// src/switch/TDMASwitch.cc
#include "TDMASwitch.h"
#include "../core/common/Constants.h"
#include "../messages/TDMAFrame_m.h"
#include <sstream>

Define_Module(TDMASwitch);

void TDMASwitch::initialize() {
    numPorts = par("numPorts");
    switchingDelay = par("switchingDelay");
    
    // Inizializza code per ogni porta
    for (int i = 0; i < numPorts; i++) {
        portQueues[i] = std::queue<cPacket*>();
        portBusy[i] = false;
        maxQueueDepth[i] = 0;
    }
    
    // Carica MAC table
    loadMacTable();
    
    EV << "=== TDMASwitch " << getName() << " initialized ===" << endl;
    EV << "Ports: " << numPorts << ", MAC entries: " << macTable.size() << endl;
}

void TDMASwitch::loadMacTable() {
    std::string config = par("macTableConfig").stringValue();
    if (config.empty()) return;
    
    std::stringstream ss(config);
    std::string entry;
    
    while (std::getline(ss, entry, ',')) {
        // Rimuovi spazi
        entry.erase(0, entry.find_first_not_of(" \t"));
        entry.erase(entry.find_last_not_of(" \t") + 1);
        
        size_t arrowPos = entry.find("->");
        if (arrowPos == std::string::npos) continue;
        
        std::string mac = entry.substr(0, arrowPos);
        std::string portsStr = entry.substr(arrowPos + 2);
        
        // Rimuovi spazi dal MAC
        mac.erase(0, mac.find_first_not_of(" \t"));
        mac.erase(mac.find_last_not_of(" \t") + 1);
        
        // Parse ports (separated by ;)
        std::stringstream pss(portsStr);
        std::string portToken;
        std::vector<int> ports;
        
        while (std::getline(pss, portToken, ';')) {
             try {
                ports.push_back(std::stoi(portToken));
             } catch (...) {
                 EV_ERROR << "Invalid port in config: " << portToken << endl;
             }
        }

        if (!ports.empty()) {
            macTable[mac] = ports;
            EV_DEBUG << "MAC " << mac << " -> ports " << ports.size() << " [";
            for(int p : ports) EV_DEBUG << p << " ";
            EV_DEBUG << "]" << endl;
        }
    }
}

void TDMASwitch::handleMessage(cMessage *msg) {
    if (msg->isSelfMessage()) {
        handleSelfMessage(msg);
    } else {
        handleIncomingFrame(check_and_cast<cPacket*>(msg));
    }
}

void TDMASwitch::handleIncomingFrame(cPacket *pkt) {
    int arrivalPort = pkt->getArrivalGate()->getIndex();
    
    TDMAFrame *frame = dynamic_cast<TDMAFrame*>(pkt);
    if (!frame) {
        EV_WARN << "Non-TDMA frame received, dropping" << endl;
        delete pkt;
        return;
    }
    
    EV_DEBUG << "Frame received on port " << arrivalPort 
             << ": " << frame->getSrcAddr() << " -> " << frame->getDstAddr() << endl;
    
    // Simula switching delay
    cMessage *processMsg = new cMessage("ProcessFrame");
    processMsg->setContextPointer(frame);
    processMsg->setKind(arrivalPort);
    
    scheduleAt(simTime() + switchingDelay, processMsg);
}

void TDMASwitch::handleSelfMessage(cMessage *msg) {
    if (strcmp(msg->getName(), "ProcessFrame") == 0) {
        TDMAFrame *frame = static_cast<TDMAFrame*>(msg->getContextPointer());
        int arrivalPort = msg->getKind();
        delete msg;
        
        processAndForward(frame, arrivalPort);
        
    } else if (strcmp(msg->getName(), "TxComplete") == 0) {
        int port = msg->getKind();
        delete msg;
        
        portBusy[port] = false;
        
        // Trasmetti prossimo frame in coda
        if (!portQueues[port].empty()) {
            transmitFrame(port);
        }
    }
}

void TDMASwitch::processAndForward(TDMAFrame *frame, int arrivalPort) {
    std::string dstMac = frame->getDstAddr();
    std::string srcMac = frame->getSrcAddr();

    EV << "Switch " << getName() << ": frame " << srcMac 
       << " -> " << dstMac << " (port " << arrivalPort << ")" << endl;
    
    // --- MAC LEARNING ---
    // Se non conosciamo il MAC sorgente, lo impariamo associandolo alla porta di arrivo
    if (macTable.find(srcMac) == macTable.end()) {
        macTable[srcMac] = {arrivalPort};
        EV << "  [MAC Learning] Learned " << srcMac << " -> port " << arrivalPort << endl;
    } else {
        // Opzionale: aggiorna se cambiato (es. mobilità, ma qui è statico)
    }

    // --- FORWARDING ---
    std::vector<int> destPorts;
    
    auto it = macTable.find(dstMac);
    if (it != macTable.end()) {
        // Destinazione nota (Unicast o Multicast)
        destPorts = it->second;
        EV << "  -> forwarding to " << destPorts.size() << " ports" << endl;
    } else {
        // Destinazione sconosciuta: FLOODING (tranne porta arrivo)
        EV_WARN << "MAC " << dstMac << " unknown -> FLOODING" << endl;
        for (int i = 0; i < numPorts; i++) {
            if (i != arrivalPort) {
                destPorts.push_back(i);
            }
        }
    }
    
    // Invia su tutte le porte identificate
    for (int destPort : destPorts) {
        // Previeni loop (non rispedire indietro)
        if (destPort == arrivalPort) continue;
        
        // Duplica frame per ogni porta (tranne l'ultima per efficienza, ma qui duplichiamo sempre per sicurezza)
        TDMAFrame *copy = frame->dup();
        
        // Accoda
        portQueues[destPort].push(copy);
        
        // Stats
        int qSize = portQueues[destPort].size();
        if (qSize > maxQueueDepth[destPort]) {
            maxQueueDepth[destPort] = qSize;
        }
        emit(registerSignal("queueLength"), qSize);
        
        // Trasmetti se libero
        if (!portBusy[destPort]) {
            transmitFrame(destPort);
        }
    }
    
    // Il frame originale non serve più (abbiamo inviato copie o scartato)
    delete frame;
}

void TDMASwitch::transmitFrame(int port) {
    if (portQueues[port].empty()) return;
    
    cPacket *frame = portQueues[port].front();
    portQueues[port].pop();
    
    portBusy[port] = true;
    
    // Calcola tempo di trasmissione
    uint64_t bits = frame->getBitLength();
    simtime_t txTime = SimTime((double)bits / tdma::DATARATE, SIMTIME_S);
    
    EV_DEBUG << "Transmitting on port " << port 
             << " (" << bits << " bits, " << txTime << ")" << endl;
    
    // Invia frame
    send(frame, "port$o", port);
    
    // Schedula fine trasmissione
    cMessage *txComplete = new cMessage("TxComplete");
    txComplete->setKind(port);
    scheduleAt(simTime() + txTime, txComplete);
}

void TDMASwitch::finish() {
    EV << "=== Switch " << getName() << " Statistics ===" << endl;
    
    for (int i = 0; i < numPorts; i++) {
        if (maxQueueDepth[i] > 0) {
            EV << "Port " << i << " max queue: " << maxQueueDepth[i] << endl;
            recordScalar(("port" + std::to_string(i) + "_maxQueue").c_str(), maxQueueDepth[i]);
        }
    }
}