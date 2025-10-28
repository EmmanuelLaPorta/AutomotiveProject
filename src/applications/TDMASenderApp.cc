#include "PeriodicTrafficGen.h"
#include "AppPackets_m.h"
#include "EthernetFrame_m.h"

Define_Module(PeriodicTrafficGen);

void PeriodicTrafficGen::initialize()
{
    period = par("period");
    startTime = par("startTime");
    name = par("name").str();
    payloadSize = par("payloadSize");
    burstSize = par("burstSize");
    destAddr = par("destAddr").str();
    srcAddr = par("srcAddr").str();

    if(startTime > 0) {
        cMessage *timer = new cMessage("TxTimer");
        scheduleAt(startTime, timer);
    }
}

void PeriodicTrafficGen::handleMessage(cMessage *msg)
{
    if(msg->isSelfMessage()) {
        if(strcmp(msg->getName(), "TxTimer") == 0) {
            transmitPacket();
            scheduleAt(simTime()+par("period"), msg);
            return;
        }

        error("E' arrivato un self message non previsto");
    }

    DataPacket *pkt = check_and_cast<DataPacket *>(msg);
    if(strcmp(pkt->getName(), name.c_str()) != 0) {
        delete pkt;
        return;
    }


    EV_DEBUG << "Arrivato pacchetto no. " << pkt->getPktNumber()
            << ", di " << pkt->getBurstSize() << endl;
    simtime_t delay = simTime() - pkt->getGenTime();

    simsignal_t sig = registerSignal("E2EDelay");
    emit(sig, delay);

    if(pkt->getPktNumber() == pkt->getBurstSize()) {
        sig = registerSignal("E2EBurstDelay");
        emit(sig, delay);
    }

    delete pkt;
}

void PeriodicTrafficGen::transmitPacket() {
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
