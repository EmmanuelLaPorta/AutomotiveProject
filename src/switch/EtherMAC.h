#ifndef __AUTOMOTIVETDMANETWORK_ETHERMAC_H_
#define __AUTOMOTIVETDMANETWORK_ETHERMAC_H_

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

    typedef enum {
        RX_STATE_IDLE,
        RX_STATE_RX
    } rx_state_t;

    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;  // ✅ Per statistiche finali

    virtual void startTransmission();
    virtual void startReception();  // ✅ NUOVO
    virtual bool vlanFilter(cPacket *pkt);

    tx_state_t txstate;
    rx_state_t rxstate;  // ✅ NUOVO
    cPacketQueue txqueue;
    cPacketQueue rxqueue;  // ✅ NUOVA CODA RX
    cPacket *rxbuf;
    uint64_t datarate;
    simtime_t ifgdur;
    std::vector<int> vlans;
    
    // Statistiche
    int maxTxQueueSize;
    int maxRxQueueSize;
};

#endif