// src/switch/TDMASwitch.h
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
    
    // MAC Table: MAC Address -> Vector of Output Ports (for Multicast)
    std::map<std::string, std::vector<int>> macTable;
    
    // Priority Queuing: Port -> Priority -> Queue
    // Priority 0 = Highest, 7 = Lowest
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