#include "AppDispatcher.h"

Define_Module(AppDispatcher);

void AppDispatcher::initialize()
{
    // Nessuna inizializzazione necessaria
}

void AppDispatcher::handleMessage(cMessage *msg)
{
    if(msg->getArrivalGate()->isName("upperLayerIn")) {
        // Messaggio da applicazione -> invia alla rete
        send(msg, "lowerLayerOut");
        return;
    }

    // Messaggio dalla rete -> broadcast a tutte le applicazioni
    int numApps = gateSize("upperLayerOut");
    for(int i = 0; i < numApps; i++) {
        cMessage *copy = msg->dup();
        send(copy, "upperLayerOut", i);
    }
    delete msg;
}