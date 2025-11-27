// Implementazione sender
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
    
    // Parse slot list dal parametro
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
    currentFragment = 0;
    hyperperiod = par("hyperperiod");
    cycleCount = 0;
    
    EV << "=== TDMASenderApp " << flowId << " ===" << endl;
    EV << "Slots: " << txSlots.size() << ", Fragments: " << burstSize << endl;
    
    if (!txSlots.empty()) {
        scheduleNextSlot();
    }
}

void TDMASenderApp::handleMessage(cMessage *msg) {
    if (msg->isSelfMessage() && strcmp(msg->getName(), "TxSlot") == 0) {
        delete msg;
        sendFragment();
    }
}

void TDMASenderApp::sendFragment() {
    std::string currentDst = dstAddr;
    
    // Round-robin per multicast
    if (dstAddr == "multicast") {
        if (flowId.find("flow2") != std::string::npos) {
            // Audio: S1, S2, S3, S4
            std::vector<std::string> speakers = {
                "00:00:00:00:00:05", "00:00:00:00:00:08", 
                "00:00:00:00:00:0D", "00:00:00:00:00:11"
            };
            currentDst = speakers[currentFragment % 4]; 
        } else if (flowId.find("flow6") != std::string::npos) {
            // Video: RS1, RS2
            std::vector<std::string> screens = {
                "00:00:00:00:00:12", "00:00:00:00:00:0E"
            };
            currentDst = screens[currentFragment % 2];
        }
    }

    // Flag ultimo frammento del burst corrente
    bool isLast = ((currentFragment + 1) % burstSize == 0); 

    TDMAFrame *frame = new TDMAFrame(flowId.c_str());
    frame->setSrcAddr(srcAddr.c_str());
    frame->setDstAddr(currentDst.c_str());
    frame->setFlowId(flowId.c_str());
    frame->setSlotNumber(currentSlot);
    frame->setFragmentNumber(currentFragment % burstSize);
    frame->setTotalFragments(burstSize);
    frame->setGenTime(simTime());
    frame->setTxTime(txDuration);
    frame->setLastFragment(isLast);
    frame->setByteLength(payloadSize);

    send(frame, "out");
    packetsSent++;

    EV_DEBUG << flowId << " frag " << (currentFragment % burstSize) 
             << " -> " << currentDst << " @ " << simTime() << endl;

    currentFragment++;
    currentSlot++;
    
    scheduleNextSlot();
}

void TDMASenderApp::scheduleNextSlot() {
    // Se abbiamo esaurito gli slot di questo ciclo, passa al prossimo
    if (currentSlot >= txSlots.size()) {
        currentSlot = 0;
        cycleCount++;
        currentFragment = 0;  // Reset frammenti per nuovo ciclo
    }

    while (currentSlot < txSlots.size()) {
        // Offset originale + shift per il ciclo corrente
        simtime_t nextTime = txSlots[currentSlot] + (hyperperiod * cycleCount);
        
        if (nextTime >= simTime()) {
            cMessage *slotMsg = new cMessage("TxSlot");
            scheduleAt(nextTime, slotMsg);
            return;
        }
        
        // Slot nel passato, salta
        EV_WARN << flowId << " skipped slot " << currentSlot
                << " @ " << nextTime << endl;
        currentSlot++;
    }

    // Se siamo qui, tutti gli slot del ciclo erano nel passato
    // Riprova col prossimo ciclo
    scheduleNextSlot();
}

void TDMASenderApp::finish() {
    recordScalar("packetsSent", packetsSent);
    EV << flowId << " sent " << packetsSent << " packets" << endl;
}
