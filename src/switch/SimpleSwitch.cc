// SimpleSwitch.cc
#include "SimpleSwitch.h"
#include "../messages/EthernetFrame_m.h"

Define_Module(SimpleSwitch);

void SimpleSwitch::initialize() {
    datarate = par("datarate").doubleValue();
    switchingDelay = par("switchingDelay");
    
    int numPorts = gateSize("port");

    
    for (int i = 0; i < numPorts; i++) {
        portBusy[i] = false;
        maxQueueSize[i] = 0;
    }
    
    EV << "SimpleSwitch inizializzato con " << numPorts << " porte" << endl;
    EV << "  Datarate: " << datarate << " bps" << endl;
    EV << "  Switching Delay: " << switchingDelay << " s" << endl;
}

void SimpleSwitch::handleMessage(cMessage *msg) {
    if (msg->isSelfMessage()) {
        // Timer di trasmissione completata
        int port = msg->getKind();
        delete msg;
        
        portBusy[port] = false;
        
        // Se c'è altro in coda, trasmetti
        if (!portQueues[port].empty()) {
            transmitFromPort(port);
        }
        
        return;
    }
    
    // Frame in arrivo
    cPacket *frame = check_and_cast<cPacket*>(msg);
    int arrivalPort = frame->getArrivalGate()->getIndex();
    
    // Applica switching delay
    scheduleAt(simTime() + switchingDelay, msg);
}

void SimpleSwitch::processFrame(cPacket *pkt, int arrivalPort) {
    EthernetFrame *frame = dynamic_cast<EthernetFrame*>(pkt);
    if (!frame) {
        delete pkt;
        return;
    }
    
    std::string srcMac = frame->getSrc();
    std::string dstMac = frame->getDst();
    
    // MAC Learning
    if (macTable.find(srcMac) == macTable.end()) {
        macTable[srcMac] = arrivalPort;
        EV_DEBUG << "MAC Learning: " << srcMac << " → port " << arrivalPort << endl;
    }
    
    // Determina porta di destinazione
    int destPort = lookupPort(dstMac);
    
    if (destPort == -1 || destPort == arrivalPort) {
        // Flooding o loop
        delete frame;
        return;
    }
    
    // Accoda alla porta di uscita
    portQueues[destPort].push(frame);
    
    int qSize = portQueues[destPort].size();
    if (qSize > maxQueueSize[destPort]) {
        maxQueueSize[destPort] = qSize;
    }
    
    EV_DEBUG << "Frame accodato sulla porta " << destPort 
             << " (queue=" << qSize << ")" << endl;
    
    // Se la porta è libera, inizia trasmissione
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
    
    // Calcola tempo di trasmissione
    simtime_t txTime = SimTime(frame->getBitLength() / datarate, SIMTIME_S);
    
    // Invia il frame
    send(frame, "port$o", port);
    
    // Schedula fine trasmissione
    cMessage *txComplete = new cMessage("TxComplete");
    txComplete->setKind(port);
    scheduleAt(simTime() + txTime, txComplete);
    
    EV_DEBUG << "Trasmissione su porta " << port 
             << ", durata=" << txTime << " s" << endl;
}

int SimpleSwitch::lookupPort(const std::string& dstMac) {
    if (dstMac == "FF:FF:FF:FF:FF:FF") {
        return -1;  // Broadcast
    }
    
    auto it = macTable.find(dstMac);
    if (it != macTable.end()) {
        return it->second;
    }
    
    return -1;  // Unknown
}

void SimpleSwitch::finish() {
    EV << "=== SimpleSwitch Statistics ===" << endl;
    
    for (const auto& pair : maxQueueSize) {
        EV << "  Port " << pair.first << " Max Queue: " << pair.second << endl;
        recordScalar((std::string("port") + std::to_string(pair.first) + "_maxQueue").c_str(), 
                     pair.second);
    }
    
    EV << "  MAC Table Entries: " << macTable.size() << endl;
}
