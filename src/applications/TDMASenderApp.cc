#include "TDMASenderApp.h"
#include "../messages/AppPackets_m.h"
#include "../messages/EthernetFrame_m.h"

Define_Module(TDMASenderApp);

void TDMASenderApp::initialize()
{
    period = par("period");
    tdmaOffset = par("tdmaOffset");
    name = par("name").str();
    payloadSize = par("payloadSize");
    burstSize = par("burstSize");
    destAddr = par("destAddr").str();
    srcAddr = par("srcAddr").str();

    // Schedule primo invio con offset TDMA
    simtime_t startTime = tdmaOffset;
    if(startTime >= 0) {
        cMessage *timer = new cMessage("TxTimer");
        scheduleAt(startTime, timer);
    }
}

void TDMASenderApp::handleMessage(cMessage *msg)
{
    if(msg->isSelfMessage()) {
        if(strcmp(msg->getName(), "TxTimer") == 0) {
            transmitPacket();
            scheduleAt(simTime() + period, msg);
            return;
        }

        error("Arrivato un self message non previsto");
    }

    // Questo modulo invia solo, non riceve
    error("TDMASenderApp non dovrebbe ricevere messaggi dalla rete");
}

void TDMASenderApp::transmitPacket() {
    DataPacket *pkt = new DataPacket(name.c_str());
    pkt->setByteLength(payloadSize);
    pkt->setGenTime(simTime());
    pkt->setBurstSize(burstSize);

    for(int i=0; i<burstSize; i++) {
        DataPacket *toSend = pkt->dup();

        EthTransmitReq *req = new EthTransmitReq();
        req->setSrc(srcAddr.c_str());
        req->setDst(destAddr.c_str());
        toSend->setControlInfo(req);

        toSend->setPktNumber(i+1);
        send(toSend, "lowerLayerOut");
    }

    delete pkt;
}