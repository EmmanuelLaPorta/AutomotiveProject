// src/nodes/components/applications/TDMAReceiverApp.h
#ifndef TDMA_RECEIVER_APP_H
#define TDMA_RECEIVER_APP_H

#include <omnetpp.h>
#include <string>

using namespace omnetpp;

class TDMAReceiverApp : public cSimpleModule {
protected:
    std::string flowId;
    
    // Statistics
    long packetsReceived;
    simtime_t lastPacketTime;
    simtime_t minDelay;
    simtime_t maxDelay;
    simtime_t totalDelay;
    simtime_t totalJitter;
    
    // Signals
    simsignal_t delaySignal;
    simsignal_t jitterSignal;
    simsignal_t throughputSignal;
    
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
};

#endif