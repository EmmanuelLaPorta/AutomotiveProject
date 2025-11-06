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

    EV << "=== TDMASenderApp [" << name << "] ===" << endl;
    EV << "Periodo: " << period << endl;
    EV << "TDMA Offset: " << tdmaOffset << endl;
    EV << "Fragment TX Time: " << fragmentTxTime << endl;
    EV << "Burst Size: " << burstSize << " frammenti" << endl;

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
                // GUARD TIME SINCRONIZZATO: 96ns IFG + 1000ns guard
                const simtime_t IFG = SimTime(96, SIMTIME_NS);
                const simtime_t GUARD_TIME = SimTime(1000, SIMTIME_NS);
                
                cMessage *nextTimer = new cMessage("FragmentTimer");
                nextTimer->setKind(fragmentNumber + 1);
                scheduleAt(simTime() + fragmentTxTime + IFG + GUARD_TIME, nextTimer);
                
                EV_DEBUG << "Prossimo frammento " << (fragmentNumber+1) 
                         << " schedulato a t=" << (simTime() + fragmentTxTime + IFG + GUARD_TIME) << endl;
            } else {
                // Burst completo, attendi il prossimo periodo
                cMessage *nextTimer = new cMessage("FragmentTimer");
                nextTimer->setKind(1);
                scheduleAt(simTime() + period, nextTimer);
                
                EV << "Burst completo, prossimo a t=" << (simTime() + period) << endl;
            }
            
            delete msg;
            return;
        }
    }
}

void TDMASenderApp::sendFragment(int fragmentNumber)
{
    EV_DEBUG << "[" << name << "] Invio frammento " << fragmentNumber 
             << "/" << burstSize << " a t=" << simTime() << endl;

    DataPacket *pkt = new DataPacket(name.c_str());
    pkt->setByteLength(payloadSize);
    pkt->setGenTime(simTime());
    pkt->setBurstSize(burstSize);
    pkt->setPktNumber(fragmentNumber);

    EthTransmitReq *req = new EthTransmitReq();
    req->setSrc(srcAddr.c_str());
    req->setDst(destAddr.c_str());
    pkt->setControlInfo(req);

    send(pkt, "lowerLayerOut");
}