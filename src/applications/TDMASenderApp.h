// TDMASenderApp.h
#ifndef TDMA_SENDER_APP_H
#define TDMA_SENDER_APP_H

#include <omnetpp.h>
#include <vector>
#include <sstream>

using namespace omnetpp;

class TDMASenderApp : public cSimpleModule {
protected:
    std::string name;
    int payloadSize;
    std::string destAddr;
    std::string srcAddr;
    simtime_t period;
    
    std::vector<simtime_t> txSlots;
    int currentSlot;
    int totalPacketsSent;
    
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    
    void sendPacket(int pktNumber);
};

#endif