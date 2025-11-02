#include "EtherMAC.h"
#include "../messages/EthernetFrame_m.h"

Define_Module(EtherMAC);

void EtherMAC::initialize()
{
    txstate = TX_STATE_IDLE;
    rxstate = RX_STATE_IDLE;
    rxbuf = nullptr;
    datarate = par("datarate");
    txqueue = cPacketQueue();
    rxqueue = cPacketQueue();  // ✅ NUOVA CODA RX
    ifgdur = 96.0/(double)datarate;

    // Statistiche per debug
    maxRxQueueSize = 0;
    maxTxQueueSize = 0;
    WATCH(maxRxQueueSize);
    WATCH(maxTxQueueSize);

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
        } 
        else if(strcmp(msg->getName(), "IFGTimer") == 0) {
            delete msg;
            startTransmission();
        } 
        else if(strcmp(msg->getName(), "RxTimer") == 0) {
            delete msg;
            
            // Fine ricezione
            if(rxbuf->hasBitError()) {
                EV_DEBUG << "Frame errata ricevuta, scartata" << endl;
                delete rxbuf;
                rxbuf = nullptr;
            } else {
                send(rxbuf, "upperLayerOut");
                rxbuf = nullptr;
            }
            
            rxstate = RX_STATE_IDLE;
            
            // ✅ Processa il prossimo pacchetto in coda RX se presente
            if(rxqueue.getLength() > 0) {
                startReception();
            }
        }
        return;
    }

    cPacket *pkt = check_and_cast<cPacket *>(msg);
    
    // ===== TRASMISSIONE (da upper layer) =====
    if(pkt->getArrivalGate() == gate("upperLayerIn")) {
        if(vlanFilter(pkt)) {
            EV_DEBUG << "VlanId non registrato, pacchetto scartato" << endl;
            delete msg;
            return;
        }

        txqueue.insert(pkt);
        
        // Traccia dimensione massima coda TX
        if(txqueue.getLength() > maxTxQueueSize) {
            maxTxQueueSize = txqueue.getLength();
        }

        if(txstate == TX_STATE_IDLE) {
            startTransmission();
        }
        return;
    }

    // ===== RICEZIONE (da canale) =====
    // ✅ GESTIONE COLLISIONI: Se stiamo già ricevendo, metti in coda
    if(rxstate != RX_STATE_IDLE) {
        EV_WARN << "Collisione in ricezione! Pacchetto accodato (rxqueue=" 
                << rxqueue.getLength() << ")" << endl;
        rxqueue.insert(pkt);
        
        // Traccia dimensione massima coda RX
        if(rxqueue.getLength() > maxRxQueueSize) {
            maxRxQueueSize = rxqueue.getLength();
        }
        return;
    }

    // Inizia ricezione immediata
    rxbuf = pkt;
    rxstate = RX_STATE_RX;
    
    simtime_t rxdur = (double)pkt->getBitLength()/(double)datarate;
    cMessage *rxtim = new cMessage("RxTimer");
    scheduleAt(simTime()+rxdur, rxtim);
}

void EtherMAC::startReception()
{
    if(rxqueue.getLength() == 0) {
        rxstate = RX_STATE_IDLE;
        return;
    }

    rxbuf = check_and_cast<cPacket*>(rxqueue.pop());
    rxstate = RX_STATE_RX;
    
    simtime_t rxdur = (double)rxbuf->getBitLength()/(double)datarate;
    cMessage *rxtim = new cMessage("RxTimer");
    scheduleAt(simTime()+rxdur, rxtim);
    
    EV_DEBUG << "Inizio ricezione pacchetto dalla coda RX" << endl;
}

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

void EtherMAC::finish()
{
    // Stampa statistiche finali
    EV << "=== EtherMAC Statistics ===" << endl;
    EV << "Max TX Queue Size: " << maxTxQueueSize << endl;
    EV << "Max RX Queue Size: " << maxRxQueueSize << endl;
    
    // Registra come scalari
    recordScalar("maxTxQueueSize", maxTxQueueSize);
    recordScalar("maxRxQueueSize", maxRxQueueSize);
}