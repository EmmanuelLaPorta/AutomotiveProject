// src/core/scheduler/TDMAScheduler.h
#ifndef TDMA_SCHEDULER_H
#define TDMA_SCHEDULER_H

#include <omnetpp.h>
#include <vector>
#include <map>
#include <string>

using namespace omnetpp;

class TDMAScheduler : public cSimpleModule {
public:
    struct Flow {
        std::string id;
        std::string src;
        std::string dst;
        std::string srcMac;
        std::string dstMac;
        simtime_t period;
        int payload;
        int priority;
        std::vector<std::string> path;
        simtime_t txTime;
        bool isFragmented = false;
        int fragmentCount = 1;
    };
    
    enum SlotType {
        SLOT_SENDER,
        SLOT_SWITCH
    };
    
    struct Slot {
        std::string flowId;
        std::string node;
        simtime_t offset;
        simtime_t duration;
        SlotType type;
    };

protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    
private:
    simtime_t hyperperiod;
    double datarate;
    double guardTime; // <--- QUESTA ERA LA VARIABILE MANCANTE
    
    std::vector<Flow> flows;
    std::vector<Slot> schedule;
    
    void defineFlows();
    void generateOptimizedSchedule();
    bool verifyNoCollisions();
    void configureSenders();
    void configureSwitches();
    simtime_t calculateTxTime(int payloadBytes);
    void printScheduleDebug();
    std::vector<std::string> getPathTo(const std::string& src, const std::string& dst);
};

#endif