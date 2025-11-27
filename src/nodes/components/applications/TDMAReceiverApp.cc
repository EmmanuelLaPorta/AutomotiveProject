// Implementazione receiver
#include "TDMAReceiverApp.h"
#include "../../../messages/TDMAFrame_m.h"

Define_Module(TDMAReceiverApp);

void TDMAReceiverApp::initialize() {
    flowId = par("flowId").stringValue();
    
    maxDelayTotal = 0;
    maxJitterTotal = 0;
    lastPacketTimeTotal = -1;

    delayVectorTotal = new cOutVector("e2eDelay_TOTAL");
    jitterVectorTotal = new cOutVector("jitter_TOTAL");
    
    EV << "TDMAReceiverApp " << getFullPath() << " initialized" << endl;
}

void TDMAReceiverApp::handleMessage(cMessage *msg) {
    TDMAFrame *frame = check_and_cast<TDMAFrame*>(msg);
    
    // Filtra per flow ID se receiver dedicato
    if (!flowId.empty() && strcmp(frame->getFlowId(), flowId.c_str()) != 0) {
        EV_DEBUG << "Receiver scarta frame flowId=" << frame->getFlowId()
                 << " (atteso: " << flowId << ")" << endl;
        delete frame;
        return;
    }
    
    std::string frameFlowId = frame->getFlowId();

    // Registrazione dinamica vector per nuovo flow
    if (delayVectors.find(frameFlowId) == delayVectors.end()) {
        delayVectors[frameFlowId] = new cOutVector(("e2eDelay_" + frameFlowId).c_str());
        jitterVectors[frameFlowId] = new cOutVector(("jitter_" + frameFlowId).c_str());
        maxDelayPerFlow[frameFlowId] = 0;
        maxJitterPerFlow[frameFlowId] = 0;
        lastPacketTimePerFlow[frameFlowId] = -1;
        EV << "Registrati vector per flow: " << frameFlowId << endl;
    }

    // Calcolo E2E delay
    simtime_t delay = simTime() - frame->getGenTime();
    
    if (delay > maxDelayPerFlow[frameFlowId]) {
        maxDelayPerFlow[frameFlowId] = delay;
    }
    if (delay > maxDelayTotal) {
        maxDelayTotal = delay;
    }

    delayVectors[frameFlowId]->record(delay);
    delayVectorTotal->record(delay);

    // Calcolo jitter PDV: |delay_curr - delay_prev|
    if (lastDelayMap.find(frameFlowId) != lastDelayMap.end()) {
         double prevDelay = lastDelayMap[frameFlowId];
         simtime_t jitter = fabs((delay.dbl() - prevDelay));
         
         if (jitter > maxJitterPerFlow[frameFlowId]) {
            maxJitterPerFlow[frameFlowId] = jitter;
         }
         jitterVectors[frameFlowId]->record(jitter);
         EV_DEBUG << frameFlowId << " delay=" << delay << ", jitter=" << jitter << endl;
    }
    lastDelayMap[frameFlowId] = delay.dbl();

    // Jitter totale basato su inter-arrival time
    if (lastPacketTimeTotal >= 0) {
        simtime_t interArrival = simTime() - lastPacketTimeTotal;
        simtime_t expectedInterval = frame->getTxTime();
        simtime_t jitter = fabs((interArrival - expectedInterval).dbl());

        if (jitter > maxJitterTotal) {
            maxJitterTotal = jitter;
        }
        jitterVectorTotal->record(jitter);
    }
    
    lastPacketTimePerFlow[frameFlowId] = simTime();
    lastPacketTimeTotal = simTime();
    
    delete frame;
}

void TDMAReceiverApp::finish() {
    // Scalari per-flow
    for (const auto& entry : maxDelayPerFlow) {
        const std::string& fid = entry.first;
        recordScalar(("maxDelay_" + fid).c_str(), maxDelayPerFlow[fid]);
        recordScalar(("maxJitter_" + fid).c_str(), maxJitterPerFlow[fid]);
        EV << "Flow " << fid << ": maxDelay=" << maxDelayPerFlow[fid]
           << ", maxJitter=" << maxJitterPerFlow[fid] << endl;
    }

    // Scalari aggregati
    recordScalar("maxDelay_TOTAL", maxDelayTotal);
    recordScalar("maxJitter_TOTAL", maxJitterTotal);

    EV << "=== " << getFullPath() << " TOTAL ===" << endl;
    EV << "Max Delay: " << maxDelayTotal << ", Max Jitter: " << maxJitterTotal << endl;

    // Cleanup
    for (auto& entry : delayVectors) delete entry.second;
    for (auto& entry : jitterVectors) delete entry.second;
    delete delayVectorTotal;
    delete jitterVectorTotal;
}
