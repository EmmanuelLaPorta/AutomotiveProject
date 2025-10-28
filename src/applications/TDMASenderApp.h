#ifndef __RTN_01_PERIODICTRAFFICGEN_H_
#define __RTN_01_PERIODICTRAFFICGEN_H_

#include <omnetpp.h>

using namespace omnetpp;
using namespace std;

class PeriodicTrafficGen : public cSimpleModule {
  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;

    virtual void transmitPacket();

    simtime_t period;
    simtime_t startTime;
    string name;
    unsigned long long payloadSize;
    unsigned int burstSize;
    string destAddr;
    string srcAddr;
};

#endif
