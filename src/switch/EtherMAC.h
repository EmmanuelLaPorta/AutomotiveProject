#ifndef __AUTOMOTIVETDMANETWORK_ETHERMAC_H_
#define __AUTOMOTIVETDMANETWORK_ETHERMAC_H_

#include <omnetpp.h>
#include <map>
#include <vector>

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

    // ✅ NUOVO: Struttura per TDMA slot degli switch
    struct TDMASlot {
        simtime_t offset;
        std::string flowName;
        int fragmentNumber;
    };

    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

    virtual void startTransmission();
    virtual void startReception();
    virtual bool vlanFilter(cPacket *pkt);
    
    // ✅ NUOVO: Gestione TDMA per switch
    virtual void scheduleTDMATransmissions();
    virtual bool canTransmitNow(cPacket *pkt);

    tx_state_t txstate;
    rx_state_t rxstate;
    cPacketQueue txqueue;
    cPacketQueue rxqueue;
    cPacket *rxbuf;
    uint64_t datarate;
    simtime_t ifgdur;
    std::vector<int> vlans;
    
    // ✅ NUOVO: TDMA scheduling per switch
    bool isTDMAEnabled;
    std::vector<TDMASlot> tdmaSlots;
    size_t currentSlotIndex;
    cMessage *tdmaTimer;
    
    // Statistiche
    int maxTxQueueSize;
    int maxRxQueueSize;
    int tdmaBlockedPackets; // Pacchetti bloccati per TDMA
};

#endif