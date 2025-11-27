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
    std::string dstAddr;        // MAC specifico o "multicast"
    int payloadSize;            // Byte per frammento
    int burstSize;              // Frammenti totali (per header)
    int numDestinations;        // Destinazioni multicast
    simtime_t txDuration;
    
    std::vector<simtime_t> txSlots;  // Offset slot da scheduler
    int currentSlot;
    long packetsSent;
    int currentFragment;        // Contatore frammenti inviati
    simtime_t hyperperiod;
    int cycleCount;
    
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

private:
    void sendFragment();
    void scheduleNextSlot();
};

#endif
