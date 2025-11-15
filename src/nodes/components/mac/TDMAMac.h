// src/nodes/components/mac/TDMAMac.h
#ifndef TDMA_MAC_H
#define TDMA_MAC_H

#include <omnetpp.h>
#include <string>

using namespace omnetpp;

class TDMAMac : public cSimpleModule {
protected:
    enum TxState { TX_IDLE, TX_BUSY };
    enum RxState { RX_IDLE, RX_BUSY };
    
    cPacketQueue txQueue;
    cPacketQueue rxQueue;
    
    double datarate;
    std::string macAddress;
    
    TxState txState;
    RxState rxState;
    
    cPacket *currentRxFrame;
    
    int maxTxQueueSize;
    int maxRxQueueSize;
    
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    
private:
    void handleSelfMessage(cMessage *msg);
    void handleUpperMessage(cPacket *pkt);
    void handleLowerMessage(cPacket *pkt);
    void startTransmission();
    void processNextRx();
};

#endif