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
    
    // Filtra per flow ID - se flowId è vuoto, accetta tutto
    if (!flowId.empty() && strcmp(frame->getFlowId(), flowId.c_str()) != 0) {
        EV_WARN << "Receiver " << getFullPath() << " scarta frame flowId=" 
                << frame->getFlowId() << " (atteso: " << flowId << ")" << endl; // DEBUG
        delete frame;
        return;
    }
    
    EV << "RICEVUTO frame con flowId: " << frame->getFlowId() 
       << " dst=" << frame->getDstAddr() << endl; // DEBUG
	
    // Calcola delay end-to-end
    simtime_t delay = simTime() - frame->getGenTime();
    
    // Aggiorna statistiche
    packetsReceived++;
    totalDelay += delay;
    
    if (delay < minDelay) minDelay = delay;
    if (delay > maxDelay) maxDelay = delay;
    
    // Calcola jitter (variazione del delay)
    if (lastPacketTime >= 0) {
        simtime_t interArrival = simTime() - lastPacketTime;
        simtime_t expectedInterval = frame->getTxTime();
        simtime_t jitter = fabs((interArrival - expectedInterval).dbl());
        
        totalJitter += jitter;
        emit(jitterSignal, jitter);
        
        EV_DEBUG << flowId << " fragment " << frame->getFragmentNumber()
                 << ": delay=" << delay << ", jitter=" << jitter << endl;
    }
    
    lastPacketTime = simTime();
    
    // Emetti metriche
    emit(delaySignal, delay);
    
    // Se ultimo frammento del burst, calcola throughput
    if (frame->getLastFragment()) {
        double throughput = (frame->getTotalFragments() * frame->getByteLength() * 8) / 
                          frame->getTxTime().dbl();
        emit(throughputSignal, throughput);
    }
    
    delete frame;
}

void TDMAReceiverApp::finish() {
    if (packetsReceived > 0) {
        simtime_t avgDelay = totalDelay / packetsReceived;
        simtime_t avgJitter = (packetsReceived > 1) ? 
                              totalJitter / (packetsReceived - 1) : 0;
        
        recordScalar("packetsReceived", packetsReceived);
        recordScalar("avgDelay", avgDelay);
        recordScalar("minDelay", minDelay);
        recordScalar("maxDelay", maxDelay);
        recordScalar("avgJitter", avgJitter);
        
        EV << "=== " << flowId << " Receiver Statistics ===" << endl;
        EV << "Packets: " << packetsReceived << endl;
        EV << "Avg Delay: " << avgDelay << endl;
        EV << "Min/Max Delay: " << minDelay << "/" << maxDelay << endl;
        EV << "Avg Jitter: " << avgJitter << endl;
    }
}
