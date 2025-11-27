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
        std::string src;          // Nodo sorgente
        std::string dst;          // Destinazione/i (virgola-separated per multicast)
        std::string srcMac;
        std::string dstMac;       // "multicast" per flussi multicast
        simtime_t period;         // Periodo di trasmissione
        int payload;              // Payload totale in byte
        int priority;             // Priorita EDF (0=max)
        std::vector<std::string> path;  // Path fisico attraverso switch
        simtime_t txTime;         // Tempo TX calcolato (payload + overhead)
        bool isFragmented = false;
        int fragmentCount = 1;    // Numero frammenti se payload > MTU
    };
    
    enum SlotType {
        SLOT_SENDER,    // Slot assegnato a nodo sorgente
        SLOT_SWITCH     // Slot assegnato a switch intermedio
    };
    
    // Entry nella tabella di scheduling
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
    
    void defineFlows();              // Definisce gli 8 flussi da specifica
    void generateOptimizedSchedule();// Algoritmo EDF pipelined
    bool verifyNoCollisions();
    void configureSenders();         // Inietta slot nei TDMASenderApp
    void configureSwitches();        // Configura MAC table degli switch
    simtime_t calculateTxTime(int payloadBytes);
    void printScheduleDebug();
    std::vector<std::string> getPathTo(const std::string& src, const std::string& dst);
};

#endif
