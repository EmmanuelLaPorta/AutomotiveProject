#include "EthEncap.h"
#include "EthernetFrame_m.h"

Define_Module(EthEncap);

void EthEncap::initialize()
{
    address = par("address").str();
}

void EthEncap::handleMessage(cMessage *msg)
{
    if(msg->getArrivalGate() == gate("upperLayerIn")) {
        EthTransmitReq *req = dynamic_cast<EthTransmitReq *>(msg->removeControlInfo());
        if(req == nullptr) {
            error("Un messaggio applicativo richiede EthTransmitReq");
        }

        /* Gestire il messaggio arrivato dai livelli superiori */
        return;
    }

    /* Qui gestire il messaggio arrivato dalla rete */
    /* Filtrarlo se non è destinato al nodo altrimenti
     * inviare solo il payload ai livelli superiori */
}
