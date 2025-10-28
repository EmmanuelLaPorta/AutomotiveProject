#ifndef __AUTOMOTIVETDMANETWORK_TDMASENDERAPP_H_
#define __AUTOMOTIVETDMANETWORK_TDMASENDERAPP_H_

#include <omnetpp.h>

using namespace omnetpp;
using namespace std;

class TDMASenderApp : public cSimpleModule {
  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;

    virtual void transmitPacket();

    simtime_t period;
    simtime_t tdmaOffset;  // NUOVO: offset TDMA per lo slot
    string name;
    unsigned long long payloadSize;
    unsigned int burstSize;
    string destAddr;
    string srcAddr;
};

#endif