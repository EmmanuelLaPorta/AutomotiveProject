#include "TDMASenderApp.h"
#include "../messages/AppPackets_m.h"
#include "../messages/EthernetFrame_m.h"

Define_Module(TDMASenderApp);

void TDMASenderApp::initialize()
{
    period = par("period");
    tdmaOffset = par("tdmaOffset");
    fragmentTxTime = par("fragmentTxTime");
    name = par("name").str();
    payloadSize = par("payloadSize");
    burstSize = par("burstSize");
    destAddr = par("destAddr").str();
    srcAddr = par("srcAddr").str();
    
    currentBurstNumber = 0;
    nextFragmentToSend = 0;

    EV << "=== TDMASenderApp Inizializzato ===" << endl;
    EV << "Nome: " << name << endl;
    EV << "Periodo: " << period << endl;
    EV << "TDMA Offset: " << tdmaOffset << endl;
    EV << "Fragment TX Time: " << fragmentTxTime << endl;
    EV << "Payload Size: " << payloadSize << " B" << endl;
    EV << "Burst Size: " << burstSize << " frammenti" << endl;

    // ✅ SOLUZIONE: Schedule SOLO il primo frammento
    // Gli altri seguiranno automaticamente secondo il periodo
    if(tdmaOffset >= 0) {
        cMessage *timer = new cMessage("FragmentTimer");
        timer->setKind(1);  // Primo frammento
        scheduleAt(tdmaOffset, timer);
        EV << "Primo frammento schedulato a t=" << tdmaOffset << endl;
    }
}

void TDMASenderApp::handleMessage(cMessage *msg)
{
    if(msg->isSelfMessage()) {
        if(strcmp(msg->getName(), "FragmentTimer") == 0) {
            int fragmentNumber = msg->getKind();
            
            sendFragment(fragmentNumber);
            
            if(fragmentNumber < burstSize) {
                // ? CORRETTO: Prossimo frammento dopo fragmentTxTime + guard
                cMessage *nextTimer = new cMessage("FragmentTimer");
                nextTimer->setKind(fragmentNumber + 1);
                scheduleAt(simTime() + fragmentTxTime + SimTime(596, SIMTIME_NS), nextTimer);
            } else {
                // Burst completo, prossimo burst dopo periodo
                cMessage *nextTimer = new cMessage("FragmentTimer");
                nextTimer->setKind(1);
                scheduleAt(simTime() + period, nextTimer);
            }
            
            delete msg;
            return;
        }
    }
}

void TDMASenderApp::sendFragment(int fragmentNumber)
{
    EV_DEBUG << "Invio frammento " << fragmentNumber << "/" << burstSize 
             << " a t=" << simTime() << endl;

    // Crea il pacchetto dati
    DataPacket *pkt = new DataPacket(name.c_str());
    pkt->setByteLength(payloadSize);
    pkt->setGenTime(simTime());
    pkt->setBurstSize(burstSize);
    pkt->setPktNumber(fragmentNumber);

    // Crea la richiesta di trasmissione Ethernet
    EthTransmitReq *req = new EthTransmitReq();
    req->setSrc(srcAddr.c_str());
    req->setDst(destAddr.c_str());
    pkt->setControlInfo(req);

    // Invia il frammento
    send(pkt, "lowerLayerOut");
}
