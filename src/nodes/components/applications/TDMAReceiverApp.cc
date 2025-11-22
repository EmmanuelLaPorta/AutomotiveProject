// src/nodes/components/applications/TDMAReceiverApp.cc
#include "TDMAReceiverApp.h"
#include "../../../messages/TDMAFrame_m.h"

Define_Module(TDMAReceiverApp);

void TDMAReceiverApp::initialize() {
    flowId = par("flowId").stringValue();
    
    maxDelayTotal = 0;
    maxJitterTotal = 0;
    lastPacketTimeTotal = -1;

    // Crea vector aggregati TOTAL
    delayVectorTotal = new cOutVector("e2eDelay_TOTAL");
    jitterVectorTotal = new cOutVector("jitter_TOTAL");
    
    EV << "TDMAReceiverApp " << getFullPath() << " initialized" << endl;
}

void TDMAReceiverApp::handleMessage(cMessage *msg) {
    TDMAFrame *frame = check_and_cast<TDMAFrame*>(msg);
    
    // Filtra per flow ID se specificato (receiver dedicati)
    if (!flowId.empty() && strcmp(frame->getFlowId(), flowId.c_str()) != 0) {
        EV_DEBUG << "Receiver scarta frame flowId=" << frame->getFlowId()
                 << " (atteso: " << flowId << ")" << endl;
        delete frame;
        return;
    }
    
    std::string frameFlowId = frame->getFlowId();

    // Registrazione dinamica per questo flow (solo prima volta)
    if (delayVectors.find(frameFlowId) == delayVectors.end()) {
        delayVectors[frameFlowId] = new cOutVector(("e2eDelay_" + frameFlowId).c_str());
        jitterVectors[frameFlowId] = new cOutVector(("jitter_" + frameFlowId).c_str());

        maxDelayPerFlow[frameFlowId] = 0;
        maxJitterPerFlow[frameFlowId] = 0;
        lastPacketTimePerFlow[frameFlowId] = -1;

        EV << "Registrati vector per flow: " << frameFlowId << endl;
    }

    // Calcola delay end-to-end
    simtime_t delay = simTime() - frame->getGenTime();
    
    // Aggiorna max delay per questo flow
    if (delay > maxDelayPerFlow[frameFlowId]) {
        maxDelayPerFlow[frameFlowId] = delay;
    }
    
    // Aggiorna max delay totale
    if (delay > maxDelayTotal) {
        maxDelayTotal = delay;
    }

    // Registra delay nei vector
    delayVectors[frameFlowId]->record(delay);
    delayVectorTotal->record(delay);

    // Calcola jitter (solo se non primo pacchetto del flow)
    if (lastPacketTimePerFlow[frameFlowId] >= 0) {
        simtime_t interArrival = simTime() - lastPacketTimePerFlow[frameFlowId];
        simtime_t expectedInterval = frame->getTxTime();
        simtime_t jitter = fabs((interArrival - expectedInterval).dbl());
        
        // Aggiorna max jitter per questo flow
        if (jitter > maxJitterPerFlow[frameFlowId]) {
            maxJitterPerFlow[frameFlowId] = jitter;
        }

        // Registra jitter nei vector
        jitterVectors[frameFlowId]->record(jitter);
        
        EV_DEBUG << frameFlowId << " delay=" << delay << ", jitter=" << jitter << endl;
    }

    // Calcola jitter totale (se non primo pacchetto assoluto)
    if (lastPacketTimeTotal >= 0) {
        simtime_t interArrival = simTime() - lastPacketTimeTotal;
        simtime_t expectedInterval = frame->getTxTime();
        simtime_t jitter = fabs((interArrival - expectedInterval).dbl());

        // Aggiorna max jitter totale
        if (jitter > maxJitterTotal) {
            maxJitterTotal = jitter;
        }

        // Registra jitter totale
        jitterVectorTotal->record(jitter);
    }
    
    lastPacketTimePerFlow[frameFlowId] = simTime();
    lastPacketTimeTotal = simTime();
    
    delete frame;
}

void TDMAReceiverApp::finish() {
    // Statistiche per-flow (SOLO MAX)
    for (const auto& entry : maxDelayPerFlow) {
        const std::string& flowId = entry.first;
        
        recordScalar(("maxDelay_" + flowId).c_str(), maxDelayPerFlow[flowId]);
        recordScalar(("maxJitter_" + flowId).c_str(), maxJitterPerFlow[flowId]);
        
        EV << "Flow " << flowId << ": maxDelay=" << maxDelayPerFlow[flowId]
           << ", maxJitter=" << maxJitterPerFlow[flowId] << endl;
    }

    // Statistiche aggregate (TOTAL)
    recordScalar("maxDelay_TOTAL", maxDelayTotal);
    recordScalar("maxJitter_TOTAL", maxJitterTotal);

    EV << "=== " << getFullPath() << " TOTAL Statistics ===" << endl;
    EV << "Max Delay: " << maxDelayTotal << endl;
    EV << "Max Jitter: " << maxJitterTotal << endl;

    // Cleanup vectors
    for (auto& entry : delayVectors) {
        delete entry.second;
    }
    for (auto& entry : jitterVectors) {
        delete entry.second;
    }
    delete delayVectorTotal;
    delete jitterVectorTotal;
}
