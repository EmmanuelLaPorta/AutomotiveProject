#ifndef __AUTOMOTIVETDMANETWORK_TDMASENDERAPP_H_
#define __AUTOMOTIVETDMANETWORK_TDMASENDERAPP_H_

#include <omnetpp.h>

using namespace omnetpp;
using namespace std;

class TDMASenderApp : public cSimpleModule {
  protected:
    std::string name;
    int payloadSize;
    int burstSize;
    std::string destAddr;
    std::string srcAddr;

    // Nuova gestione della schedulazione
    std::string tdmaOffsets; // Stringa ricevuta dal TDMAScheduler
    std::vector<simtime_t> scheduled_offsets; // Vettore degli offset di trasmissione
    int fragmentsSentInBurst;

    // Numero del burst corrente (per gestire il ciclo sull'iperperiodo)
    int currentBurstNumber;

    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void sendFragment(int fragmentNumber);
};

#endif
