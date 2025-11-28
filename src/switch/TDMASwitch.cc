// Implementazione switch
#include "TDMASwitch.h"
#include "../core/common/Constants.h"
#include "../messages/TDMAFrame_m.h"
#include <sstream>

Define_Module(TDMASwitch);

void TDMASwitch::initialize() {
    numPorts = par("numPorts");
    switchingDelay = par("switchingDelay");
    
    for (int i = 0; i < numPorts; i++) {
        portBusy[i] = false;
        maxQueueDepth[i] = 0;
    }
    
    loadMacTable();
    
    EV << "=== TDMASwitch " << getName() << " ===" << endl;
    EV << "Ports: " << numPorts << ", MAC entries: " << macTable.size() << endl;
}

// Parse configurazione MAC table da stringa
// Formato: "MAC1->port1;port2,MAC2->port3,..."
void TDMASwitch::loadMacTable() {
    std::string config = par("macTableConfig").stringValue();
    if (config.empty()) return;
    
    std::stringstream ss(config);
    std::string entry;
    
    while (std::getline(ss, entry, ',')) {
        entry.erase(0, entry.find_first_not_of(" \t"));
        entry.erase(entry.find_last_not_of(" \t") + 1);
        
        size_t arrowPos = entry.find("->");
        if (arrowPos == std::string::npos) continue;
        
        std::string mac = entry.substr(0, arrowPos);
        std::string portsStr = entry.substr(arrowPos + 2);
        
        mac.erase(0, mac.find_first_not_of(" \t"));
        mac.erase(mac.find_last_not_of(" \t") + 1);
        
        // Parse porte (separate da ;)
        std::stringstream pss(portsStr);
        std::string portToken;
        std::vector<int> ports;
        
        while (std::getline(pss, portToken, ';')) {
             try {
                ports.push_back(std::stoi(portToken));
             } catch (...) {
                 EV_ERROR << "Invalid port: " << portToken << endl;
             }
        }

        if (!ports.empty()) {
            macTable[mac] = ports;
            EV_DEBUG << "MAC " << mac << " -> ports [";
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
        EV_WARN << "Non-TDMA frame, dropping" << endl;
        delete pkt;
        return;
    }
    
    EV_DEBUG << "Rx port " << arrivalPort << ": " << frame->getSrcAddr() 
             << " -> " << frame->getDstAddr() << endl;
    
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
        transmitFrame(port);
    }
}

void TDMASwitch::processAndForward(TDMAFrame *frame, int arrivalPort) {
    std::string dstMac = frame->getDstAddr();
    std::string srcMac = frame->getSrcAddr();

    EV << getName() << ": " << srcMac << " -> " << dstMac 
       << " (port " << arrivalPort << ")" << endl;
    

    // Lookup destinazione
    std::vector<int> destPorts;
    auto it = macTable.find(dstMac);
    if (it != macTable.end()) {
        destPorts = it->second;
    } else {
        // Flooding se MAC sconosciuto
        for (int i = 0; i < numPorts; i++) {
            if (i != arrivalPort) destPorts.push_back(i);
        }
    }

    // Inoltra su tutte le porte destinazione
    for (int destPort : destPorts) {
        if (destPort == arrivalPort) continue;
        
        TDMAFrame *copy = frame->dup();
        
        portQueues[destPort].push(copy);
        
        int qSize = portQueues[destPort].size();
        if (qSize > maxQueueDepth[destPort]) {
            maxQueueDepth[destPort] = qSize;
        }
        emit(registerSignal("queueLength"), qSize);
        
        if (!portBusy[destPort]) {
            transmitFrame(destPort);
        }
    }
    
    delete frame;
}

// Trasmette frame dalla coda FIFO
void TDMASwitch::transmitFrame(int port) {
    if (portBusy[port]) return;
    
    if (!portQueues[port].empty()) {
        cPacket *frame = portQueues[port].front();
        portQueues[port].pop();
        
        portBusy[port] = true;
        
        uint64_t bits = frame->getBitLength();
        simtime_t txTime = SimTime((double)bits / tdma::DATARATE, SIMTIME_S);
        
        EV_DEBUG << "Tx port " << port << " (" << bits << " bits)" << endl;
        
        send(frame, "port$o", port);
        
        cMessage *txComplete = new cMessage("TxComplete");
        txComplete->setKind(port);
        scheduleAt(simTime() + txTime, txComplete);
    }
}

void TDMASwitch::finish() {
    EV << "=== Switch " << getName() << " Stats ===" << endl;
    
    for (int i = 0; i < numPorts; i++) {
        if (maxQueueDepth[i] > 0) {
            EV << "Port " << i << " maxQueue: " << maxQueueDepth[i] << endl;
            recordScalar(("port" + std::to_string(i) + "_maxQueue").c_str(), 
                         maxQueueDepth[i]);
        }
    }
}
