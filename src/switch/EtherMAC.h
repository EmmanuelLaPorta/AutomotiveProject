#ifndef __RTN_01_ETHERMAC_H_
#define __RTN_01_ETHERMAC_H_

#include <omnetpp.h>

using namespace omnetpp;

class EtherMAC : public cSimpleModule
{
  protected:
    typedef enum {
        TX_STATE_IDLE,
        TX_STATE_TX,
        TX_STATE_IFG
    } tx_state_t;

    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;

    virtual void startTransmission();
    virtual bool vlanFilter(cPacket *pkt);

    tx_state_t txstate;
    cPacketQueue txqueue;
    cPacket *rxbuf;
    uint64_t datarate;
    simtime_t ifgdur;
    std::vector<int> vlans;
};

#endif
