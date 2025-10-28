#include "EtherMAC.h"
#include "EthernetFrame_m.h"

Define_Module(EtherMAC);

void EtherMAC::initialize()
{
    txstate = TX_STATE_IDLE;
    rxbuf = nullptr;
    datarate = par("datarate");
    txqueue = cPacketQueue();//Aggiungere comparator per non fifo
    ifgdur = 96.0/(double)datarate;

    cValueArray *vlanArray = check_and_cast<cValueArray*>(par("vlans").objectValue());
    for (int i = 0; i < vlanArray->size(); ++i) {
        vlans.push_back((int)vlanArray->get(i).intValue());
    }
}

void EtherMAC::handleMessage(cMessage *msg)
{
    if(msg->isSelfMessage()) {
        if(strcmp(msg->getName(), "TxTimer") == 0) {
            delete msg;
            cMessage *ifgtim = new cMessage("IFGTimer");
            scheduleAt(simTime()+ifgdur, ifgtim);
            txstate = TX_STATE_IFG;
        } else if(strcmp(msg->getName(), "IFGTimer") == 0) {
            delete msg;
            startTransmission();
        } else if(strcmp(msg->getName(), "RxTimer") == 0) {
            delete msg;
            if(rxbuf->hasBitError()) {
                EV_DEBUG << "Ricevuta frame errata la rimuovo!" << endl;
                delete rxbuf;
                rxbuf = nullptr;
                return;
            }

            send(rxbuf, "upperLayerOut");
            rxbuf = nullptr;
        }
        return;
    }

    cPacket *pkt = check_and_cast<cPacket *>(msg);
    if(pkt->getArrivalGate() == gate("upperLayerIn")) {
        if(vlanFilter(pkt)) {
            EV_DEBUG << "VlanId non registrato" << endl;
            delete msg;
            return;
        }

        txqueue.insert(pkt);
        if(txstate == TX_STATE_IDLE) {
            startTransmission();
        }
        return;
    }

    //Arriva da channelIn
    if(rxbuf != nullptr) {
        error("Non possono esserci due ricezioni contemporanee!");
    }
    rxbuf = pkt;
    simtime_t rxdur = (double)pkt->getBitLength()/(double)datarate;
    cMessage *rxtim = new cMessage("RxTimer");
    scheduleAt(simTime()+rxdur, rxtim);
}

/**
 * Ritorna true se la frame deve essere droppata
 * a causa del vlan id, altrimenti false
 */
bool EtherMAC::vlanFilter(cPacket *pkt) {
    if(vlans.size() == 0) {
        return false;
    }

    EthernetQFrame *qf = dynamic_cast<EthernetQFrame *>(pkt);
    if(qf == nullptr) {
        return false;
    }

    for(int i=0; i<vlans.size(); i++) {
        if(vlans[i] == qf->getVlanid()) {
            return false;
        }
    }

    return true;

}

void EtherMAC::startTransmission() {
    if(txqueue.getLength() == 0) {
        txstate = TX_STATE_IDLE;
        return;
    }

    cPacket *pkt = txqueue.pop();
    simtime_t txdur = (double)pkt->getBitLength()/(double)datarate;
    send(pkt, "channelOut");
    cMessage *txtim = new cMessage("TxTimer");
    scheduleAt(simTime()+txdur, txtim);
    txstate = TX_STATE_TX;
}
