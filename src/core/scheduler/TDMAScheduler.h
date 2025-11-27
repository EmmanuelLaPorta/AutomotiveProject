#ifndef TDMA_SCHEDULER_H
#define TDMA_SCHEDULER_H

#include <omnetpp.h>
#include <vector>
#include <map>
#include <string>

using namespace omnetpp;

class TDMAScheduler : public cSimpleModule {
public:
    // Definizione di un flusso di traffico
    struct Flow {
        std::string id;           // ID univoco (es "flow1_LD1")
        std::string src;          // Nome nodo sorgente (es "LD1")
        std::string dst;          // Nome nodi destinazione (comma-separated es "HU" o "S1,S2")
        std::string srcMac;
        std::string dstMac;       
        simtime_t period;         // Periodo di trasmissione
        int payload;              // Payload del frammento in byte
        int priority;             // Priorita EDF (0=max)
        std::vector<std::string> path;  
        simtime_t txTime;         // Tempo TX calcolato
        bool isFragmented = false;
        int fragmentCount = 1;    // Numero frammenti
    };
    
    enum SlotType {
        SLOT_SENDER,    
        SLOT_SWITCH     
    };
    
    struct Slot {
        std::string flowId;
        std::string node;         // Nodo che trasmette in questo slot
        simtime_t offset;         // Offset dall'inizio dell'hyperperiod
        simtime_t duration;
        SlotType type;
    };

protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    
private:
    simtime_t hyperperiod;
    double datarate;
    double guardTime;
    
    std::vector<Flow> flows;
    std::vector<Slot> schedule;
    
    void discoverFlowsFromNetwork(); // Legge i parametri .ini dai moduli
    void generateOptimizedSchedule();// Algoritmo EDF pipelined
    void configureSenders();         // Inietta slot nei TDMASenderApp
    void configureSwitches();        // Configura MAC table degli switch
    
    simtime_t calculateTxTime(int payloadBytes);
    std::vector<std::string> getPathTo(const std::string& src, const std::string& dst);
};

#endif