#ifndef TDMA_RECEIVER_APP_H
#define TDMA_RECEIVER_APP_H

#include <omnetpp.h>
#include <string>
#include <map>

using namespace omnetpp;

class TDMAReceiverApp : public cSimpleModule {
protected:
    std::string flowId;  // Vuoto = accetta tutti i flow
    
    // Statistiche flow
    std::map<std::string, simtime_t> maxDelayPerFlow;
    std::map<std::string, simtime_t> maxJitterPerFlow;
    std::map<std::string, simtime_t> lastPacketTimePerFlow;
    std::map<std::string, double> lastDelayMap; 

    // Statistiche aggregate
    simtime_t maxDelayTotal;
    simtime_t maxJitterTotal;
    simtime_t lastPacketTimeTotal;

    // Output vectors flow
    std::map<std::string, cOutVector*> delayVectors;
    std::map<std::string, cOutVector*> jitterVectors;
    
    // Output vectors aggregati
    cOutVector *delayVectorTotal;
    cOutVector *jitterVectorTotal;
    
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
};

#endif
