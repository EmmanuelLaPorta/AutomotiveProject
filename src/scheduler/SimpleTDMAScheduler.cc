// SimpleTDMAScheduler.cc
#include "SimpleTDMAScheduler.h"

Define_Module(SimpleTDMAScheduler);

void SimpleTDMAScheduler::initialize() {
    hyperperiod = par("hyperperiod");
    datarate = par("datarate").doubleValue();
    ethernetOverhead = par("ethernetOverhead").intValue();

    EV << "=== SimpleTDMAScheduler Initialization ===" << endl;
    EV << "Hyperperiod: " << hyperperiod << " s" << endl;
    EV << "Datarate: " << datarate << " bps" << endl;

    configureFlow1();
    
    EV << "=== TDMA Schedule Completato ===" << endl;
}

void SimpleTDMAScheduler::handleMessage(cMessage *msg) {
    error("SimpleTDMAScheduler non dovrebbe ricevere messaggi");
}

simtime_t SimpleTDMAScheduler::calculateTxTime(int payloadBytes) {
    int totalBytes = payloadBytes + ethernetOverhead;
    uint64_t totalBits = totalBytes * 8;
    return SimTime(totalBits / datarate, SIMTIME_S);
}

void SimpleTDMAScheduler::configureFlow1() {
    EV << "Configurazione FLOW 1: LD1 → CU" << endl;
    
    // Parametri Flusso 1
    simtime_t period = SimTime(1.4e-3, SIMTIME_S);  // 1.4 ms
    int payloadSize = 1300;  // bytes
    simtime_t txTime = calculateTxTime(payloadSize);
    
    EV << "  Periodo: " << period << " s" << endl;
    EV << "  Payload: " << payloadSize << " B" << endl;
    EV << "  Tempo TX: " << txTime << " s (" << (txTime.dbl()*1e6) << " us)" << endl;
    
    // Calcola numero di trasmissioni nell'iperperiodo
    int numTransmissions = (int)floor(hyperperiod.dbl() / period.dbl());
    EV << "  Trasmissioni in hyperperiod: " << numTransmissions << endl;
    
    // Genera gli slot TDMA (semplicemente ogni periodo)
    std::stringstream offsetsStr;
    
    for (int i = 0; i < numTransmissions; i++) {
        simtime_t offset = i * period;
        
        if (i > 0) offsetsStr << ",";
        offsetsStr << offset.dbl();
        
        if (i < 5) {  // Log primi 5
            EV << "    Slot " << i << ": t=" << offset << " s" << endl;
        }
    }
    
    // Configura il sender LD1
    cModule *ld1 = getSystemModule()->getSubmodule("LD1");
    if (ld1) {
        cModule *senderApp = ld1->getSubmodule("senderApp", 0);
        if (senderApp) {
            senderApp->par("tdmaOffsets").setStringValue(offsetsStr.str());
            senderApp->par("fragmentTxTime").setDoubleValue(txTime.dbl());
            
            EV << "  LD1.senderApp[0] configurato con " << numTransmissions << " slot" << endl;
        }
    }
    
    // Configura il sender LD2
        cModule *ld2 = getSystemModule()->getSubmodule("LD2");
        if (ld2) {
            cModule *senderApp = ld2->getSubmodule("senderApp", 0);
            if (senderApp) {
                senderApp->par("tdmaOffsets").setStringValue(offsetsStr.str());
                senderApp->par("fragmentTxTime").setDoubleValue(txTime.dbl());

                EV << "  LD1.senderApp[0] configurato con " << numTransmissions << " slot" << endl;
            }
        }

    // Calcola utilizzo canale
    simtime_t totalTxTime = numTransmissions * txTime;
    double utilization = (totalTxTime.dbl() / hyperperiod.dbl()) * 100.0;
    
    EV << "  Utilizzo canale: " << utilization << " %" << endl;
}
