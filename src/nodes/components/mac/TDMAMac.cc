// src/nodes/components/mac/TDMAMac.cc
#include "TDMAMac.h"
#include "../../../messages/TDMAFrame_m.h"
#include "../../../core/common/Constants.h"

Define_Module(TDMAMac);

void TDMAMac::initialize() {
    txQueue = cPacketQueue("txQueue");
    rxQueue = cPacketQueue("rxQueue");
    
    datarate = par("datarate").doubleValue();
    macAddress = par("macAddress").stringValue();
    
    txState = TX_IDLE;
    rxState = RX_IDLE;
    
    maxTxQueueSize = 0;
    maxRxQueueSize = 0;
    
    WATCH(maxTxQueueSize);
    WATCH(maxRxQueueSize);
}

void TDMAMac::handleMessage(cMessage *msg) {
    if (msg->isSelfMessage()) {
        handleSelfMessage(msg);
    } else if (msg->getArrivalGate()->isName("upperIn")) {  // Cambiato da == gate("upperIn")
        handleUpperMessage(check_and_cast<cPacket*>(msg));
    } else {
        handleLowerMessage(check_and_cast<cPacket*>(msg));
    }
}
void TDMAMac::handleSelfMessage(cMessage *msg) {
    if (strcmp(msg->getName(), "TxComplete") == 0) {
        delete msg;
        txState = TX_IDLE;
        
        // Check for more packets
        if (!txQueue.isEmpty()) {
            startTransmission();
        }
        
    } else if (strcmp(msg->getName(), "RxComplete") == 0) {
        delete msg;
        rxState = RX_IDLE;
        
        // Forward to upper layer
        if (currentRxFrame) {
            send(currentRxFrame, "upperOut");
            currentRxFrame = nullptr;
        }
        
        // Process next in queue
        if (!rxQueue.isEmpty()) {
            processNextRx();
        }
    }
}

void TDMAMac::handleUpperMessage(cPacket *pkt) {
    // Add to TX queue
    txQueue.insert(pkt);
    
    int qSize = txQueue.getLength();
    if (qSize > maxTxQueueSize) {
        maxTxQueueSize = qSize;
    }
    
    emit(registerSignal("txQueueLength"), qSize);
    
    // Start transmission if idle
    if (txState == TX_IDLE) {
        startTransmission();
    }
}

void TDMAMac::handleLowerMessage(cPacket *pkt) {
    if (rxState != RX_IDLE) {
        // Queue if busy
        rxQueue.insert(pkt);
        
        int qSize = rxQueue.getLength();
        if (qSize > maxRxQueueSize) {
            maxRxQueueSize = qSize;
        }
        
        emit(registerSignal("rxQueueLength"), qSize);
    } else {
        // Process immediately
        currentRxFrame = pkt;
        rxState = RX_BUSY;
        
        // Simulate processing time
        simtime_t procTime = SimTime(pkt->getBitLength() / datarate, SIMTIME_S);
        scheduleAt(simTime() + procTime, new cMessage("RxComplete"));
    }
}

void TDMAMac::startTransmission() {
    if (txQueue.isEmpty()) {
        txState = TX_IDLE;
        return;
    }
    
    cPacket *pkt = check_and_cast<cPacket*>(txQueue.pop());
    txState = TX_BUSY;
    
    // Calculate transmission time
    simtime_t txTime = SimTime(pkt->getBitLength() / datarate, SIMTIME_S);
    
    // Send to physical layer
    send(pkt, "lowerOut");
    
    // Schedule completion
    scheduleAt(simTime() + txTime, new cMessage("TxComplete"));
    
    emit(registerSignal("txQueueLength"), txQueue.getLength());
}

void TDMAMac::processNextRx() {
    if (!rxQueue.isEmpty()) {
        currentRxFrame = check_and_cast<cPacket*>(rxQueue.pop());
        rxState = RX_BUSY;
        
        simtime_t procTime = SimTime(currentRxFrame->getBitLength() / datarate, SIMTIME_S);
        scheduleAt(simTime() + procTime, new cMessage("RxComplete"));
        
        emit(registerSignal("rxQueueLength"), rxQueue.getLength());
    }
}

void TDMAMac::finish() {
    recordScalar("maxTxQueueSize", maxTxQueueSize);
    recordScalar("maxRxQueueSize", maxRxQueueSize);
    
    EV << "MAC " << macAddress << " - Max TX Queue: " << maxTxQueueSize 
       << ", Max RX Queue: " << maxRxQueueSize << endl;
}
