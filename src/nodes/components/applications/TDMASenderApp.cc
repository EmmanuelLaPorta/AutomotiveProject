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
    
    // burstSize viene letto solo per marcare il TotalFragments nell'header,
    // non per ciclare l'invio. Lo scheduling gestisce i singoli slot.
    burstSize = par("burstSize"); 
    numDestinations = par("numDestinations");
    
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
    currentFragment = 0; // Indice progressivo assoluto
    
    EV << "=== TDMASenderApp " << flowId << " initialized ===" << endl;
    EV << "Slots: " << txSlots.size() << ", Total Fragments info: " << burstSize << endl;
    
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
    
    // Logica indirizzamento Multicast Round-Robin
    if (dstAddr == "multicast") {
        if (flowId.find("flow2") != std::string::npos) {
            // Audio: 4 speakers
            std::vector<std::string> speakers = {"00:00:00:00:00:05", "00:00:00:00:00:08", "00:00:00:00:00:0D", "00:00:00:00:00:11"};
            currentDst = speakers[currentFragment % 4]; 
        } else if (flowId.find("flow6") != std::string::npos) {
            // Video: 2 screens
            std::vector<std::string> screens = {"00:00:00:00:00:12", "00:00:00:00:00:0E"};
            currentDst = screens[currentFragment % 2];
        }
    }

    // Calcolo flag Last Fragment
    // Se burstSize > 1, è l'ultimo frammento di un gruppo
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

    EV_DEBUG << flowId << " sent fragment " << (currentFragment % burstSize) 
             << " to " << currentDst << " at " << simTime() << endl;

    currentFragment++;
    currentSlot++;
    
    if (currentSlot < txSlots.size()) {
        scheduleNextSlot();
    }
}

void TDMASenderApp::scheduleNextSlot() {
    while (currentSlot < txSlots.size()) {
        simtime_t nextTime = txSlots[currentSlot];
        
        if (nextTime >= simTime()) {
            cMessage *slotMsg = new cMessage("TxSlot");
            scheduleAt(nextTime, slotMsg);
            return;
        }
        
        // Se arriviamo qui, abbiamo mancato uno slot (non dovrebbe succedere con il nuovo scheduler)
        EV_WARN << flowId << " skipped past slot " << currentSlot << " at " << nextTime << endl;
        currentSlot++;
    }
}

void TDMASenderApp::finish() {
    recordScalar("packetsSent", packetsSent);
    EV << flowId << " sent " << packetsSent << " packets" << endl;
}