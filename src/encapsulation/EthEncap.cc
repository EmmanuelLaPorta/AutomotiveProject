#include "EthEncap.h"
#include "../messages/EthernetFrame_m.h"

Define_Module(EthEncap);

void EthEncap::initialize()
{
    address = par("address").str();
}

void EthEncap::handleMessage(cMessage *msg)
{
    if(msg->getArrivalGate() == gate("upperLayerIn")) {
        // Messaggio dai livelli superiori -> incapsula
        EthTransmitReq *req = dynamic_cast<EthTransmitReq *>(msg->removeControlInfo());
        if(req == nullptr) {
            error("Un messaggio applicativo richiede EthTransmitReq");
        }

        cPacket *payload = check_and_cast<cPacket *>(msg);
        
        // Crea frame Ethernet
        EthernetFrame *frame = new EthernetFrame(payload->getName());
        frame->setDst(req->getDst());
        frame->setSrc(req->getSrc());
        frame->setEtherType(0x0800); // IP type (non usato ma standard)
        frame->encapsulate(payload);
        
        delete req;
        send(frame, "lowerLayerOut");
        return;
    }

    // Messaggio dalla rete -> decapsula
    EthernetFrame *frame = check_and_cast<EthernetFrame *>(msg);
    
    // Filtra: accetta solo frame per questo nodo o broadcast
    if(strcmp(frame->getDst(), address.c_str()) != 0 && 
       strcmp(frame->getDst(), "FF:FF:FF:FF:FF:FF") != 0) {
        EV_DEBUG << "Frame non destinata a questo nodo, scartata" << endl;
        delete frame;
        return;
    }

    // Decapsula e invia il payload ai livelli superiori
    cPacket *payload = frame->decapsulate();
    delete frame;
    send(payload, "upperlayerOut");
}