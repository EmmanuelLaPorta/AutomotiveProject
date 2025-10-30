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
    virtual void sendFragment(int fragmentNumber);

    simtime_t period;
    simtime_t tdmaOffset;
    simtime_t fragmentTxTime;  // NUOVO: Tempo per trasmettere un frammento
    string name;
    unsigned long long payloadSize;
    unsigned int burstSize;
    string destAddr;
    string srcAddr;
    
    int currentBurstNumber;  // Contatore per burst multipli nello stesso periodo
};

#endif