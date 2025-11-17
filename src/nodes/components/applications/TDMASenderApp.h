// src/nodes/components/applications/TDMASenderApp.h
#ifndef TDMA_SENDER_APP_H
#define TDMA_SENDER_APP_H

#include <omnetpp.h>
#include <vector>
#include <string>

using namespace omnetpp;

class TDMASenderApp : public cSimpleModule {
protected:
    std::string flowId;
    std::string srcAddr;
    std::string dstAddr;
    int payloadSize;
    int burstSize;
    simtime_t txDuration;
    bool isFragmented;
    
    std::vector<simtime_t> txSlots;
    int currentSlot;
    long packetsSent;
    
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    

private:
    void transmitBurst();
    void scheduleNextSlot();
};

#endif
