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
    numDestinations = par("numDestinations");
    
    isFragmented = (burstSize > 1) && (payloadSize == 1500);

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
      EV << "Slots: " << txSlots.size() << ", Burst size: " << burstSize
         << ", Destinations: " << numDestinations << endl;
    
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
    // Flow 2: Audio multicast (ME -> 4 speaker)
    if (dstAddr == "multicast" && flowId.find("flow2") != std::string::npos) {
        std::vector<std::string> speakers = {
            "00:00:00:00:00:05",  // S1
            "00:00:00:00:00:08",  // S2
            "00:00:00:00:00:0D",  // S3
            "00:00:00:00:00:11"   // S4
        };
        
        for (size_t i = 0; i < speakers.size(); i++) {
            TDMAFrame *frame = new TDMAFrame(flowId.c_str());
            
            frame->setSrcAddr(srcAddr.c_str());
            frame->setDstAddr(speakers[i].c_str());
            frame->setFlowId(flowId.c_str());
            frame->setSlotNumber(currentSlot);
            frame->setFragmentNumber(i);
            frame->setTotalFragments(speakers.size());
            frame->setGenTime(simTime());
            frame->setTxTime(txDuration);
            frame->setLastFragment(i == speakers.size() - 1);
            frame->setByteLength(payloadSize);
            
            if (i > 0) {
                simtime_t fragmentDelay = i * (txDuration + tdma::getIfgTime());
                sendDelayed(frame, fragmentDelay, "out");
            } else {
                send(frame, "out");
            }
            
            packetsSent++;
        }
    }
    // Flow 6: Video streaming multicast frammentato (ME -> RS1, RS2)
    else if (dstAddr == "multicast" && flowId.find("flow6") != std::string::npos) {
        std::vector<std::string> screens = {
            "00:00:00:00:00:12",  // RS1
            "00:00:00:00:00:0E"   // RS2
        };

        // Determina quale destinazione inviare in questo slot
        // Slot pari (0,2,4...) -> RS1, Slot dispari (1,3,5...) -> RS2
        int destIdx = currentSlot % numDestinations;
        std::string dstMac = screens[destIdx];

        EV << "Flow6 slot " << currentSlot << ": invio " << burstSize
           << " frammenti a " << (destIdx == 0 ? "RS1" : "RS2") << endl;

        // Invia solo i frammenti per UNA destinazione
        for (int fragIdx = 0; fragIdx < burstSize; fragIdx++) {
            TDMAFrame *frame = new TDMAFrame(flowId.c_str());

            frame->setSrcAddr(srcAddr.c_str());
            frame->setDstAddr(dstMac.c_str());
            frame->setFlowId(flowId.c_str());
            frame->setSlotNumber(currentSlot);
            frame->setFragmentNumber(fragIdx);
            frame->setTotalFragments(burstSize);
            frame->setGenTime(simTime());
            frame->setTxTime(txDuration);
            frame->setLastFragment(fragIdx == burstSize - 1);
            frame->setByteLength(payloadSize);

            if (fragIdx > 0) {
                simtime_t delay = fragIdx * (txDuration + tdma::getIfgTime());
                sendDelayed(frame, delay, "out");
            } else {
                send(frame, "out");
            }

            packetsSent++;
        }
    }
    // Trasmissione normale (unicast con o senza frammentazione)
    else {
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
            
            if (i > 0) {
                simtime_t fragmentDelay = i * (txDuration + tdma::getIfgTime());
                sendDelayed(frame, fragmentDelay, "out");
            } else {
                send(frame, "out");
            }
            
            packetsSent++;
        }
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
