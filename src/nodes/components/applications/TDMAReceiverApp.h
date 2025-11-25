// src/nodes/components/applications/TDMAReceiverApp.h
#ifndef TDMA_RECEIVER_APP_H
#define TDMA_RECEIVER_APP_H

#include <omnetpp.h>
#include <string>
#include <map>

using namespace omnetpp;

class TDMAReceiverApp : public cSimpleModule {
protected:
    std::string flowId;
    
    // Statistiche per-flow
    std::map<std::string, simtime_t> maxDelayPerFlow;
    std::map<std::string, simtime_t> maxJitterPerFlow;
    std::map<std::string, simtime_t> lastPacketTimePerFlow;
    std::map<std::string, double> lastDelayMap; // Added for Jitter calculation

    // Statistiche aggregate (TOTAL)
    simtime_t maxDelayTotal;
    simtime_t maxJitterTotal;
    simtime_t lastPacketTimeTotal;

    // Output vectors per-flow
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
