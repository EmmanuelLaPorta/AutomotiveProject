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
        std::string senderModule;  // Path completo (es: "network.LD1.senderApp[0]")
        simtime_t period;
        int payloadSize;
        int burstSize;
        int priority;  // Per Rate Monotonic: priorità più alta = periodo più basso
        simtime_t txTime;  // Tempo trasmissione singolo frame
    };

    struct TransmissionSlot {
        std::string flowName;
        std::string senderModule;
        simtime_t offset;
        int fragmentNumber;
    };

    std::vector<FlowDescriptor> flows;
    std::vector<TransmissionSlot> schedule;
    
    simtime_t hyperperiod;
    double datarate;   // in bps
    int overhead;  //in bytes

    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    
    // Metodi per il calcolo dello schedule
    void defineFlows();
    void calculateTransmissionTimes();
    void generateSchedule();
    bool checkCollisions();
    void assignOffsetsToSenders();
    
    // Utility
    simtime_t calculateTxTime(int payloadSize);
    int calculateTransmissionsInHyperperiod(simtime_t period);
};

#endif
