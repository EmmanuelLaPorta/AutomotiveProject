// src/nodes/components/applications/TDMASenderApp.cc
#include "TDMASenderApp.h"
#include "../../../messages/TDMAFrame_m.h"
#include "../../../core/common/Constants.h"
#include <sstream>

Define_Module(TDMASenderApp);

void TDMASenderApp::initialize() {
    flowId = par("flowId").stringValue();
    srcAddr = par("srcAddr").stringValue();
    dstAddr = par("dstAddr").stringValue();
    payloadSize = par("payloadSize");
    burstSize = par("burstSize");
    
    // Parse TDMA slots
    std::string slotsStr = par("tdmaSlots").stringValue();
    if (!slotsStr.empty()) {
        std::stringstream ss(slotsStr);
        std::string token;
        
        while (std::getline(ss, token, ',')) {
            txSlots.push_back(SimTime(std::stod(token), SIMTIME_S));
        }
    }
    
    txDuration = par("txDuration");
    currentSlot = 0;
    packetsSent = 0;
    
    EV << "=== TDMASenderApp " << flowId << " initialized ===" << endl;
    EV << "Slots: " << txSlots.size() << ", Burst size: " << burstSize << endl;
    
    // Schedula primo slot
    if (!txSlots.empty()) {
        scheduleNextSlot();
    }
}

void TDMASenderApp::handleMessage(cMessage *msg) {
    if (msg->isSelfMessage() && strcmp(msg->getName(), "TxSlot") == 0) {
        delete msg;
        
        // Trasmetti burst
        transmitBurst();
        
        // Schedula prossimo slot
        currentSlot++;
        if (currentSlot < txSlots.size()) {
            scheduleNextSlot();
        }
    }
}

void TDMASenderApp::transmitBurst() {
    for (int i = 0; i < burstSize; i++) {
        TDMAFrame *frame = new TDMAFrame(flowId.c_str());
        
        frame->setSrcAddr(srcAddr.c_str());
        frame->setDstAddr(dstAddr.c_str());
        frame->setFlowId(flowId.c_str());
        frame->setSlotNumber(currentSlot);
        frame->setFragmentNumber(i);
        frame->setTotalFragments(burstSize);
        frame->setGenTime(simTime());
        frame->setTxTime(txDuration);
        frame->setLastFragment(i == burstSize - 1);
        frame->setByteLength(payloadSize);
        
        // Invia con piccolo delay tra frammenti
        if (i > 0) {
            simtime_t fragmentDelay = i * (txDuration + tdma::getIfgTime());
            sendDelayed(frame, fragmentDelay, "out");
        } else {
            send(frame, "out");
        }
        
        packetsSent++;
    }
    
    EV_DEBUG << flowId << " transmitted burst at slot " << currentSlot 
             << " (t=" << simTime() << ")" << endl;
}

void TDMASenderApp::scheduleNextSlot() {
    simtime_t nextTime = txSlots[currentSlot];
    
    if (nextTime < simTime()) {
        EV_WARN << "Slot " << currentSlot << " missed (was at " << nextTime << ")" << endl;
        return;
    }
    
    cMessage *slotMsg = new cMessage("TxSlot");
    scheduleAt(nextTime, slotMsg);
    
    EV_DEBUG << flowId << " scheduled slot " << currentSlot << " at " << nextTime << endl;
}

void TDMASenderApp::finish() {
    recordScalar("packetsSent", packetsSent);
    EV << flowId << " sent " << packetsSent << " packets" << endl;
}