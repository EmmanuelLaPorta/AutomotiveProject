#include "RelayUnit.h"
#include "../messages/EthernetFrame_m.h"

Define_Module(RelayUnit);

void RelayUnit::initialize()
{
    // La tabella MAC è vuota all'inizio
    WATCH_MAP(macTable);
}

void RelayUnit::handleMessage(cMessage *msg)
{
    EthernetFrame *frame = check_and_cast<EthernetFrame *>(msg);
    
    int arrivalPort = msg->getArrivalGate()->getIndex();
    std::string srcAddr = frame->getSrc();
    std::string dstAddr = frame->getDst();
    
    // ✅ MAC LEARNING: Impara l'associazione MAC → porta
    if (macTable.find(srcAddr) == macTable.end()) {
        macTable[srcAddr] = arrivalPort;
        EV_DEBUG << "MAC Learning: " << srcAddr << " → porta " << arrivalPort << endl;
    }
    
    // ✅ FORWARDING INTELLIGENTE
    if (dstAddr == "FF:FF:FF:FF:FF:FF") {
        // Broadcast: invia a tutte le porte tranne quella di arrivo
        forwardBroadcast(frame, arrivalPort);
    } else if (macTable.find(dstAddr) != macTable.end()) {
        // Unicast con MAC nota: inoltra solo alla porta corretta
        int destPort = macTable[dstAddr];
        
        if (destPort == arrivalPort) {
            // Loop detection: non reinviare sulla stessa porta
            EV_DEBUG << "Loop rilevato, frame scartato" << endl;
            delete frame;
        } else {
            EV_DEBUG << "Forwarding " << dstAddr << " → porta " << destPort << endl;
            send(frame, "portGatesOut", destPort);
        }
    } else {
        // MAC destination sconosciuta: flooding (come broadcast)
        EV_DEBUG << "MAC " << dstAddr << " sconosciuto, flooding..." << endl;
        forwardBroadcast(frame, arrivalPort);
    }
}

void RelayUnit::forwardBroadcast(EthernetFrame *frame, int arrivalPort)
{
    int numPorts = gateSize("portGatesIn");
    
    for (int i = 0; i < numPorts; i++) {
        if (i != arrivalPort) {
            EthernetFrame *copy = frame->dup();
            send(copy, "portGatesOut", i);
        }
    }
    
    delete frame;
}

void RelayUnit::finish()
{
    EV << "=== MAC Table Final State ===" << endl;
    for (const auto &entry : macTable) {
        EV << "  " << entry.first << " → porta " << entry.second << endl;
    }
}