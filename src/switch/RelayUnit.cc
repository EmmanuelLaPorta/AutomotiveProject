#include "RelayUnit.h"

Define_Module(RelayUnit);

void RelayUnit::initialize()
{
    // Nessuna inizializzazione necessaria per ora
}

void RelayUnit::handleMessage(cMessage *msg)
{
    // Ottieni l'indice della porta da cui è arrivato il messaggio
    int arrivalPort = msg->getArrivalGate()->getIndex();
    
    // Ottieni il numero totale di porte
    int numPorts = gateSize("portGatesIn");
    
    // Forward il messaggio a tutte le altre porte (tranne quella di arrivo)
    for(int i = 0; i < numPorts; i++) {
        if(i != arrivalPort) {
            cMessage *copy = msg->dup();
            send(copy, "portGatesOut", i);  // CORREZIONE: specifica l'indice
        }
    }
    
    // Elimina il messaggio originale
    delete msg;
}