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
    // Struttura per descrivere un hop nel percorso
    struct PathHop {
        std::string nodeName;      // Nome dello switch o end-system
        int ingressPort;           // Porta di ingresso (-1 per sorgente)
        int egressPort;            // Porta di uscita (-1 per destinazione)
        simtime_t hopDelay;        // Ritardo di switching/propagazione
    };

    struct FlowDescriptor {
        std::string flowName;
        std::string senderModule;
        std::string srcAddress;    // MAC sorgente
        std::string dstAddress;    // MAC destinazione
        simtime_t period;
        int payloadSize;
        int burstSize;
        int priority;
        simtime_t txTime;
        std::vector<PathHop> path;  // Percorso completo del flusso
    };

    struct TransmissionSlot {
        std::string flowName;
        std::string senderModule;
        simtime_t offset;
        int fragmentNumber;
        int burstSize;
        int instanceNumber;
        
        // NUOVO: Informazioni per coordinamento switch
        std::string switchNode;    // Quale switch (vuoto per end-system)
        int switchPort;            // Quale porta dello switch
        bool isEndSystemTx;        // true se è trasmissione da end-system
    };

    std::vector<FlowDescriptor> flows;
    std::vector<TransmissionSlot> schedule;
    
    // NUOVO: Mappa della topologia di rete
    struct NetworkTopology {
        std::map<std::string, std::vector<std::string>> connections;
        std::map<std::string, std::string> macToNode;
    };
    NetworkTopology topology;
    
    simtime_t hyperperiod;
    double datarate;
    int overhead;
    simtime_t switchingDelay;  // Ritardo di elaborazione dello switch
    simtime_t propagationDelay; // Ritardo di propagazione per hop

    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    
    void buildNetworkTopology();
    void defineFlows();
    void calculateFlowPaths();
    std::vector<PathHop> findPath(const std::string& srcMac, const std::string& dstMac);
    void calculateTransmissionTimes();
    void generateSchedule();
    bool checkCollisions();
    void assignOffsetsToSenders();
    
    simtime_t calculateTxTime(int payloadSize);
    int calculateTransmissionsInHyperperiod(simtime_t period);
    simtime_t calculateEndToEndDelay(const FlowDescriptor& flow);
    int determineEgressPort(const std::string& switchName, 
                           const std::vector<PathHop>& path, 
                           size_t currentHopIndex);
};

#endif