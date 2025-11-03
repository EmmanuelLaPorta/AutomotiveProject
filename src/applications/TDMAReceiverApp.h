#ifndef __AUTOMOTIVETDMANETWORK_TDMARECEIVERAPP_H_
#define __AUTOMOTIVETDMANETWORK_TDMARECEIVERAPP_H_

#include <omnetpp.h>

using namespace omnetpp;
using namespace std;

class TDMAReceiverApp : public cSimpleModule {
  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;

    string name;
    
    // ✅ NUOVO: Per calcolo jitter
    simtime_t lastDelay;
    bool firstPacket;
};

#endif