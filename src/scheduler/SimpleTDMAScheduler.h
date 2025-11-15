// SimpleTDMAScheduler.h
#ifndef SIMPLE_TDMA_SCHEDULER_H
#define SIMPLE_TDMA_SCHEDULER_H

#include <omnetpp.h>

using namespace omnetpp;

class SimpleTDMAScheduler : public cSimpleModule {
protected:
    simtime_t hyperperiod;
    double datarate;
    int ethernetOverhead;
    
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    
    simtime_t calculateTxTime(int payloadBytes);
    void configureFlow1();
};

#endif