// SimpleSwitch.h - VERSIONE MODULARE
#ifndef SIMPLE_SWITCH_H
#define SIMPLE_SWITCH_H

#include <omnetpp.h>
#include <queue>
#include <map>
#include <string>

using namespace omnetpp;

class SimpleSwitch : public cSimpleModule {
protected:
    double datarate;
    simtime_t switchingDelay;
    
    // Code FIFO per ogni porta
    std::map<int, std::queue<cPacket*>> portQueues;
    std::map<int, bool> portBusy;
    std::map<int, int> maxQueueSize;
    
    // MAC Learning Table: MAC address → porta
    std::map<std::string, int> macTable;
    
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    
    void loadMacTableFromParameter();  // ✅ NUOVO: Carica da parametro NED
    void processFrame(cPacket *frame, int arrivalPort);
    void transmitFromPort(int port);
    int lookupPort(const std::string& dstMac);
};

#endif