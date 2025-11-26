// src/switch/TDMASwitch.cc
#include "TDMASwitch.h"
#include "../core/common/Constants.h"
#include "../messages/TDMAFrame_m.h"
#include <sstream>

Define_Module(TDMASwitch);

void TDMASwitch::initialize() {
    numPorts = par("numPorts");
    switchingDelay = par("switchingDelay");
    
    // Inizializza stati
    for (int i = 0; i < numPorts; i++) {
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
        
        // Trasmetti prossimo frame in coda (cerca tra le priorità)
        transmitFrame(port);
    }
}

void TDMASwitch::processAndForward(TDMAFrame *frame, int arrivalPort) {
    std::string dstMac = frame->getDstAddr();
    std::string srcMac = frame->getSrcAddr();

    EV << "Switch " << getName() << ": frame " << srcMac 
       << " -> " << dstMac << " (port " << arrivalPort << ")" << endl;
    
    // --- MAC LEARNING ---
    if (macTable.find(srcMac) == macTable.end()) {
        macTable[srcMac] = {arrivalPort};
        EV << "  [MAC Learning] Learned " << srcMac << " -> port " << arrivalPort << endl;
    }

    // --- FORWARDING ---
    std::vector<int> destPorts;
    
    auto it = macTable.find(dstMac);
    if (it != macTable.end()) {
        destPorts = it->second;
    } else {
        // Flooding
        for (int i = 0; i < numPorts; i++) {
            if (i != arrivalPort) destPorts.push_back(i);
        }
    }
    
    int priority = getPriority(frame);

    for (int destPort : destPorts) {
        if (destPort == arrivalPort) continue;
        
        TDMAFrame *copy = frame->dup();
        
        // Accoda nella coda di priorità corretta
        portQueues[destPort][priority].push(copy);
        
        // Calcola dimensione totale coda per stats
        int totalQSize = 0;
        for(auto const& [prio, q] : portQueues[destPort]) {
            totalQSize += q.size();
        }
        
        if (totalQSize > maxQueueDepth[destPort]) {
            maxQueueDepth[destPort] = totalQSize;
        }
        emit(registerSignal("queueLength"), totalQSize);
        
        // Trasmetti se libero
        if (!portBusy[destPort]) {
            transmitFrame(destPort);
        }
    }
    
    delete frame;
}

void TDMASwitch::transmitFrame(int port) {
    if (portBusy[port]) return;

    // Cerca la prima coda non vuota partendo da priorità 0 (Highest)
    for (int prio = 0; prio <= 7; prio++) {
        if (!portQueues[port][prio].empty()) {
            cPacket *frame = portQueues[port][prio].front();
            portQueues[port][prio].pop();
            
            portBusy[port] = true;
            
            // Calcola tempo di trasmissione
            uint64_t bits = frame->getBitLength();
            simtime_t txTime = SimTime((double)bits / tdma::DATARATE, SIMTIME_S);
            
            EV_DEBUG << "Transmitting Prio " << prio << " on port " << port 
                     << " (" << bits << " bits, " << txTime << ")" << endl;
            
            send(frame, "port$o", port);
            
            cMessage *txComplete = new cMessage("TxComplete");
            txComplete->setKind(port);
            scheduleAt(simTime() + txTime, txComplete);
            
            return; // Trovato e trasmesso
        }
    }
}

int TDMASwitch::getPriority(TDMAFrame *frame) {
    std::string fid = frame->getFlowId();
    
    // Priority 0 (Highest) - Audio / Safety Critical
    if (fid.find("flow2") != std::string::npos) return 0; // Audio
    if (fid.find("flow7") != std::string::npos) return 1; // Telematics
    
    // Priority 2 - Sensors
    if (fid.find("flow3") != std::string::npos) return 2; // Sensors
    
    // Priority 3 - Control
    if (fid.find("flow4") != std::string::npos) return 3; // Control
    
    // Priority 4 - Video RSE (Multicast)
    if (fid.find("flow6") != std::string::npos) return 4;
    
    // Priority 5 - Video Bulk (Cameras)
    if (fid.find("flow5") != std::string::npos) return 5;
    if (fid.find("flow8") != std::string::npos) return 5;
    
    return 7; // Lowest
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