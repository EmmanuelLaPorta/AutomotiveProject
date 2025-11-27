
#ifndef TDMA_SWITCH_H
#define TDMA_SWITCH_H

#include <omnetpp.h>
#include <queue>
#include <map>
#include <string>

using namespace omnetpp;

class TDMAFrame;

class TDMASwitch : public cSimpleModule {
protected:
    int numPorts;
    simtime_t switchingDelay;
    
    // MAC -> lista porte uscita (multicast supportato)
    std::map<std::string, std::vector<int>> macTable;
    
    // Code per porta e priorita: port -> priority -> queue
    std::map<int, std::map<int, std::queue<cPacket*>>> portQueues;
    
    std::map<int, bool> portBusy;
    std::map<int, int> maxQueueDepth;
    
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    
private:
    void loadMacTable();
    void handleIncomingFrame(cPacket *pkt);
    void handleSelfMessage(cMessage *msg);
    void processAndForward(TDMAFrame *frame, int arrivalPort);
    void transmitFrame(int port);
    int getPriority(TDMAFrame *frame);
};

#endif
