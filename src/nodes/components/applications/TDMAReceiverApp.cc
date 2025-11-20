// src/nodes/components/applications/TDMAReceiverApp.cc
#include "TDMAReceiverApp.h"
#include "../../../messages/TDMAFrame_m.h"

Define_Module(TDMAReceiverApp);

void TDMAReceiverApp::initialize() {
    flowId = par("flowId").stringValue();
    
    packetsReceived = 0;
    lastPacketTime = -1;
    minDelay = SimTime::getMaxTime();
    maxDelay = 0;
    totalDelay = 0;
    totalJitter = 0;
    
    // Registra segnali statistici
    delaySignal = registerSignal("e2eDelay");
    jitterSignal = registerSignal("jitter");
    throughputSignal = registerSignal("throughput");
}

void TDMAReceiverApp::handleMessage(cMessage *msg) {
    TDMAFrame *frame = check_and_cast<TDMAFrame*>(msg);
    
    // Filtra per flow ID se specificato
    if (!flowId.empty() && strcmp(frame->getFlowId(), flowId.c_str()) != 0) {
        EV_WARN << "Receiver " << getFullPath() << " scarta frame flowId=" 
                << frame->getFlowId() << " (atteso: " << flowId << ")" << endl;
        delete frame;
        return;
    }
    
    std::string frameFlowId = frame->getFlowId();

    // Registrazione dinamica segnali per questo flow (solo prima volta)
    if (delaySignals.find(frameFlowId) == delaySignals.end()) {
        delaySignals[frameFlowId] = registerSignal(("e2eDelay_" + frameFlowId).c_str());
        jitterSignals[frameFlowId] = registerSignal(("jitter_" + frameFlowId).c_str());
        throughputSignals[frameFlowId] = registerSignal(("throughput_" + frameFlowId).c_str());

        // Crea output vectors manuali
            delayVectors[frameFlowId] = new cOutVector(("e2eDelay_" + frameFlowId).c_str());
            jitterVectors[frameFlowId] = new cOutVector(("jitter_" + frameFlowId).c_str());
            throughputVectors[frameFlowId] = new cOutVector(("throughput_" + frameFlowId).c_str());


        // Inizializza statistiche
        packetsReceivedPerFlow[frameFlowId] = 0;
        lastPacketTimePerFlow[frameFlowId] = -1;
        minDelayPerFlow[frameFlowId] = SimTime::getMaxTime();
        maxDelayPerFlow[frameFlowId] = 0;
        totalDelayPerFlow[frameFlowId] = 0;
        totalJitterPerFlow[frameFlowId] = 0;

        EV << "Registrati segnali per flow: " << frameFlowId << endl;
    }

    // Calcola delay end-to-end
    simtime_t delay = simTime() - frame->getGenTime();
    
    // Aggiorna statistiche per questo flow
    packetsReceivedPerFlow[frameFlowId]++;
    totalDelayPerFlow[frameFlowId] += delay;
    
    if (delay < minDelayPerFlow[frameFlowId]) {
        minDelayPerFlow[frameFlowId] = delay;
    }
    if (delay > maxDelayPerFlow[frameFlowId]) {
        maxDelayPerFlow[frameFlowId] = delay;
    }
    
    // Calcola jitter (solo se non primo pacchetto del flow)
    if (lastPacketTimePerFlow[frameFlowId] >= 0) {
        simtime_t interArrival = simTime() - lastPacketTimePerFlow[frameFlowId];
        simtime_t expectedInterval = frame->getTxTime();
        simtime_t jitter = fabs((interArrival - expectedInterval).dbl());
        
        totalJitterPerFlow[frameFlowId] += jitter;
        emit(jitterSignals[frameFlowId], jitter);
        emit(jitterSignal, jitter);  // SEGNALE GLOBALE
        jitterVectors[frameFlowId]->record(jitter);
        
        EV_DEBUG << frameFlowId << " fragment " << frame->getFragmentNumber()
                 << ": delay=" << delay << ", jitter=" << jitter << endl;
    }
    
    lastPacketTimePerFlow[frameFlowId] = simTime();
    
    // Emetti metriche per questo flow E globali
    emit(delaySignals[frameFlowId], delay);
    emit(delaySignal, delay);  // SEGNALE GLOBALE
    delayVectors[frameFlowId]->record(delay);
    
    // Se ultimo frammento del burst, calcola throughput
    if (frame->getLastFragment()) {
        double throughput = (frame->getTotalFragments() * frame->getByteLength() * 8) / 
                          frame->getTxTime().dbl();
        emit(throughputSignals[frameFlowId], throughput);
        emit(throughputSignal, throughput);  // SEGNALE GLOBALE
        emit(throughputSignal, throughput);
    }

    // Aggiorna statistiche globali (per backward compatibility)
    packetsReceived++;
    totalDelay += delay;
    if (delay < minDelay) minDelay = delay;
    if (delay > maxDelay) maxDelay = delay;
    if (lastPacketTime >= 0) {
        simtime_t jitter = fabs((simTime() - lastPacketTime - frame->getTxTime()).dbl());
        totalJitter += jitter;
    }
    lastPacketTime = simTime();
    
    delete frame;
}

void TDMAReceiverApp::finish() {
    // Statistiche globali (aggregate)
    if (packetsReceived > 0) {
        simtime_t avgDelay = totalDelay / packetsReceived;
        simtime_t avgJitter = (packetsReceived > 1) ? 
                              totalJitter / (packetsReceived - 1) : 0;
        
        recordScalar("packetsReceived_TOTAL", packetsReceived);
        recordScalar("avgDelay_TOTAL", avgDelay);
        recordScalar("minDelay_TOTAL", minDelay);
        recordScalar("maxDelay_TOTAL", maxDelay);
        recordScalar("avgJitter_TOTAL", avgJitter);
        
        EV << "=== " << getFullPath() << " TOTAL Statistics ===" << endl;
        EV << "Total Packets: " << packetsReceived << endl;
        EV << "Avg Delay: " << avgDelay << endl;
        EV << "Avg Jitter: " << avgJitter << endl;
    }

    // Statistiche per-flow
    EV << "=== " << getFullPath() << " Per-Flow Statistics ===" << endl;

    for (const auto& entry : packetsReceivedPerFlow) {
        const std::string& flowId = entry.first;
        long pkts = entry.second;

        if (pkts > 0) {
            simtime_t avgDelay = totalDelayPerFlow[flowId] / pkts;
            simtime_t avgJitter = (pkts > 1) ?
                                  totalJitterPerFlow[flowId] / (pkts - 1) : 0;

            // Record scalars con nome flow
            recordScalar(("packetsReceived_" + flowId).c_str(), pkts);
            recordScalar(("avgDelay_" + flowId).c_str(), avgDelay);
            recordScalar(("minDelay_" + flowId).c_str(), minDelayPerFlow[flowId]);
            recordScalar(("maxDelay_" + flowId).c_str(), maxDelayPerFlow[flowId]);
            recordScalar(("avgJitter_" + flowId).c_str(), avgJitter);

            EV << "Flow " << flowId << ":" << endl;
            EV << "  Packets: " << pkts << endl;
            EV << "  Avg Delay: " << avgDelay << endl;
            EV << "  Min/Max Delay: " << minDelayPerFlow[flowId]
               << "/" << maxDelayPerFlow[flowId] << endl;
            EV << "  Avg Jitter: " << avgJitter << endl;
        }
    }

    //Cleanup vectors
        for (auto& entry : delayVectors) {
            delete entry.second;
        }
        for (auto& entry : jitterVectors) {
            delete entry.second;
        }
        for (auto& entry : throughputVectors) {
            delete entry.second;
        }
}
