#include "TDMASenderApp.h"
#include "../messages/AppPackets_m.h"
#include "../messages/EthernetFrame_m.h"
#include <sstream>

Define_Module(TDMASenderApp);

void TDMASenderApp::initialize()
{
    name = par("name").str();
    payloadSize = par("payloadSize");
    burstSize = par("burstSize");
    destAddr = par("destAddr").str();
    srcAddr = par("srcAddr").str();
    tdmaOffsets = par("tdmaOffsets").stringValue();

    fragmentsSentInBurst = 0;

    EV << "=== TDMASenderApp [" << name << "] === (Interleaved)" << endl;
    EV << "Burst Size: " << burstSize << " frammenti" << endl;

    // Parsing degli offset dalla stringa
    std::stringstream ss(tdmaOffsets);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) {
            scheduled_offsets.push_back(SimTime(std::stod(item), SIMTIME_S));
        }
    }

    EV << "Ricevuti " << scheduled_offsets.size() << " slot di trasmissione." << endl;

    // Schedula un timer per ogni offset ricevuto
    for (size_t i = 0; i < scheduled_offsets.size(); ++i) {
        cMessage *timer = new cMessage("FragmentTimer");
        // Usiamo il kind per sapere quale frammento inviare (1-based)
        timer->setKind(i + 1);
        scheduleAt(scheduled_offsets[i], timer);
        EV_DEBUG << "Fragment " << (i+1) << " schedulato a t=" << scheduled_offsets[i] << endl;
    }
}

void TDMASenderApp::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage() && strcmp(msg->getName(), "FragmentTimer") == 0) {
        // Il kind del messaggio ci dice quale frammento del burst totale inviare
        int fragmentNumber = msg->getKind();
        sendFragment(fragmentNumber);

        delete msg;
    }
}

void TDMASenderApp::sendFragment(int fragmentNumber)
{
    EV_DEBUG << "[" << name << "] Invio frammento " << fragmentNumber 
             << "/" << scheduled_offsets.size() << " a t=" << simTime() << endl;

    DataPacket *pkt = new DataPacket(name.c_str());
    pkt->setByteLength(payloadSize);
    pkt->setGenTime(simTime());
    // Il burst size ora corrisponde al numero totale di frammenti nell'iperperiodo
    pkt->setBurstSize(scheduled_offsets.size()); 
    pkt->setPktNumber(fragmentNumber);

    EthTransmitReq *req = new EthTransmitReq();
    req->setSrc(srcAddr.c_str());
    req->setDst(destAddr.c_str());
    pkt->setControlInfo(req);

    send(pkt, "lowerLayerOut");
}