#ifndef __AUTOMOTIVETDMANETWORK_TDMASCHEDULER_H_
#define __AUTOMOTIVETDMANETWORK_TDMASCHEDULER_H_

#include <omnetpp.h>
#include <vector>
#include <map>
#include <string>

using namespace omnetpp;

class TDMAScheduler : public cSimpleModule
{
  protected:
    struct FlowDescriptor {
        std::string flowName;
        std::string senderModule;
        simtime_t period;
        int payloadSize;
        int burstSize;
        int priority;
        simtime_t txTime;
        simtime_t pathDelay; // Nuovo campo per il ritardo di propagazione del percorso
    };

    struct TransmissionSlot {
        std::string flowName;
        std::string senderModule;
        simtime_t offset;
        int fragmentNumber;
        int burstSize;  // ✅ NUOVO: Per debug
        int instanceNumber;  // ✅ NUOVO: Per tracciare quale istanza del job
    };

    std::vector<FlowDescriptor> flows;
    std::vector<TransmissionSlot> schedule;
    
    simtime_t hyperperiod;
    double datarate;
    int overhead;

    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    
    void defineFlows();
    void calculateTransmissionTimes();
    void generateSchedule();
    bool checkCollisions();
    void assignOffsetsToSenders();
    
    simtime_t calculateTxTime(int payloadSize);
    int calculateTransmissionsInHyperperiod(simtime_t period);
};

#endif