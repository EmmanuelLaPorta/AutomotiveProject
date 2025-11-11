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
    rxqueue = cPacketQueue();
    ifgdur = 96.0/(double)datarate;

    // Statistiche
    maxRxQueueSize = 0;
    maxTxQueueSize = 0;
    tdmaBlockedPackets = 0;
    WATCH(maxRxQueueSize);
    WATCH(maxTxQueueSize);
    WATCH(tdmaBlockedPackets);

    cValueArray *vlanArray = check_and_cast<cValueArray*>(par("vlans").objectValue());
    for (int i = 0; i < vlanArray->size(); ++i) {
        vlans.push_back((int)vlanArray->get(i).intValue());
    }
    
    // ✅ NUOVO: Configurazione TDMA per switch
    isTDMAEnabled = par("enableTDMA").boolValue();
    tdmaTimer = nullptr;
    currentSlotIndex = 0;
    
    if (isTDMAEnabled) {
        // Gli slot TDMA verranno configurati dal TDMAScheduler
        std::string slotsStr = par("tdmaSlots").stringValue();
        
        if (!slotsStr.empty()) {
            // Parse slot TDMA: "offset1:flowName1:fragNum1,offset2:flowName2:fragNum2,..."
            std::stringstream ss(slotsStr);
            std::string token;
            
            while (std::getline(ss, token, ',')) {
                if (token.empty()) continue;
                
                std::stringstream tokenStream(token);
                std::string offsetStr, flowName, fragNumStr;
                
                std::getline(tokenStream, offsetStr, ':');
                std::getline(tokenStream, flowName, ':');
                std::getline(tokenStream, fragNumStr, ':');
                
                TDMASlot slot;
                slot.offset = SimTime(std::stod(offsetStr), SIMTIME_S);
                slot.flowName = flowName;
                slot.fragmentNumber = std::stoi(fragNumStr);
                
                tdmaSlots.push_back(slot);
            }
            
            std::sort(tdmaSlots.begin(), tdmaSlots.end(),
                [](const TDMASlot &a, const TDMASlot &b) {
                    return a.offset < b.offset;
                });
            
            EV << "TDMA abilitato con " << tdmaSlots.size() << " slot configurati" << endl;
            scheduleTDMATransmissions();
        }
    }
    
    emit(registerSignal("txQueueLength"), 0);
    emit(registerSignal("rxQueueLength"), 0);
}

void EtherMAC::scheduleTDMATransmissions()
{
    if (!isTDMAEnabled || tdmaSlots.empty()) return;
    
    // Schedula il prossimo slot TDMA
    if (currentSlotIndex < tdmaSlots.size()) {
        if (tdmaTimer != nullptr) {
            cancelEvent(tdmaTimer);
            delete tdmaTimer;
        }
        
        tdmaTimer = new cMessage("TDMASlot");
        scheduleAt(tdmaSlots[currentSlotIndex].offset, tdmaTimer);
        
        EV_DEBUG << "Prossimo slot TDMA a t=" << tdmaSlots[currentSlotIndex].offset 
                 << " per flow " << tdmaSlots[currentSlotIndex].flowName << endl;
    }
}

bool EtherMAC::canTransmitNow(cPacket *pkt)
{
    if (!isTDMAEnabled) {
        return true; // Nessuna restrizione TDMA
    }
    
    // Controlla se siamo in uno slot TDMA valido
    if (currentSlotIndex >= tdmaSlots.size()) {
        return false; // Tutti gli slot esauriti
    }
    
    // Verifica se il pacchetto corrisponde allo slot corrente
    EthernetFrame *frame = dynamic_cast<EthernetFrame *>(pkt);
    if (frame == nullptr) return false;
    
    cPacket *encapPkt = frame->getEncapsulatedPacket();
    if (encapPkt == nullptr) return false;
    
    const TDMASlot &currentSlot = tdmaSlots[currentSlotIndex];
    
    // Confronta il nome del flusso
    if (strcmp(encapPkt->getName(), currentSlot.flowName.c_str()) == 0) {
        return true; // Match!
    }
    
    return false; // Non è il momento giusto per questo pacchetto
}

void EtherMAC::handleMessage(cMessage *msg)
{
    // ✅ NUOVO: Gestione timer TDMA
    if (msg->isSelfMessage() && strcmp(msg->getName(), "TDMASlot") == 0) {
        delete msg;
        
        // È arrivato uno slot TDMA: prova a trasmettere
        if (txstate == TX_STATE_IDLE && txqueue.getLength() > 0) {
            cPacket *pkt = check_and_cast<cPacket*>(txqueue.front());
            
            if (canTransmitNow(pkt)) {
                startTransmission();
            } else {
                EV_DEBUG << "Pacchetto in testa non corrisponde allo slot TDMA corrente" << endl;
            }
        }
        
        // Avanza allo slot successivo
        currentSlotIndex++;
        scheduleTDMATransmissions();
        return;
    }
    
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
            
            if(rxbuf->hasBitError()) {
                EV_DEBUG << "Frame errata ricevuta, scartata" << endl;
                delete rxbuf;
                rxbuf = nullptr;
            } else {
                send(rxbuf, "upperLayerOut");
                rxbuf = nullptr;
            }
            
            rxstate = RX_STATE_IDLE;
            
            if(rxqueue.getLength() > 0) {
                startReception();
            } else {
                emit(registerSignal("rxQueueLength"), 0);
            }
        }
        return;
    }

    cPacket *pkt = check_and_cast<cPacket *>(msg);
    
    // TRASMISSIONE (da upper layer)
    if(pkt->getArrivalGate() == gate("upperLayerIn")) {
        if(vlanFilter(pkt)) {
            EV_DEBUG << "VlanId non registrato, pacchetto scartato" << endl;
            delete msg;
            return;
        }

        txqueue.insert(pkt);
        
        int txQLen = txqueue.getLength();
        emit(registerSignal("txQueueLength"), txQLen);
        
        if(txQLen > maxTxQueueSize) {
            maxTxQueueSize = txQLen;
        }

        // ✅ MODIFICATO: Controlla TDMA prima di trasmettere
        if(txstate == TX_STATE_IDLE) {
            if (!isTDMAEnabled || canTransmitNow(pkt)) {
                startTransmission();
            } else {
                // Pacchetto accodato, aspetta il suo slot TDMA
                tdmaBlockedPackets++;
                EV_DEBUG << "Pacchetto accodato, aspetta slot TDMA (bloccati: " 
                         << tdmaBlockedPackets << ")" << endl;
            }
        }
        return;
    }

    // RICEZIONE (da canale)
    if(rxstate != RX_STATE_IDLE) {
        EV_WARN << "Collisione in ricezione! Pacchetto accodato (rxqueue=" 
                << rxqueue.getLength() << ")" << endl;
        rxqueue.insert(pkt);
        
        int rxQLen = rxqueue.getLength();
        emit(registerSignal("rxQueueLength"), rxQLen);
        
        if(rxQLen > maxRxQueueSize) {
            maxRxQueueSize = rxQLen;
        }
        return;
    }

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
    emit(registerSignal("rxQueueLength"), rxqueue.getLength());
    
    rxstate = RX_STATE_RX;
    
    simtime_t rxdur = (double)rxbuf->getBitLength()/(double)datarate;
    cMessage *rxtim = new cMessage("RxTimer");
    scheduleAt(simTime()+rxdur, rxtim);
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
        emit(registerSignal("txQueueLength"), 0);
        return;
    }

    cPacket *pkt = txqueue.pop();
    emit(registerSignal("txQueueLength"), txqueue.getLength());
    
    simtime_t txdur = (double)pkt->getBitLength()/(double)datarate;
    send(pkt, "channelOut");
    cMessage *txtim = new cMessage("TxTimer");
    scheduleAt(simTime()+txdur, txtim);
    txstate = TX_STATE_TX;
}

void EtherMAC::finish()
{
    EV << "=== EtherMAC Statistics ===" << endl;
    EV << "Max TX Queue Size: " << maxTxQueueSize << endl;
    EV << "Max RX Queue Size: " << maxRxQueueSize << endl;
    
    if (isTDMAEnabled) {
        EV << "TDMA Blocked Packets: " << tdmaBlockedPackets << endl;
    }
    
    recordScalar("maxTxQueueSize", maxTxQueueSize);
    recordScalar("maxRxQueueSize", maxRxQueueSize);
    
    if (isTDMAEnabled) {
        recordScalar("tdmaBlockedPackets", tdmaBlockedPackets);
    }
    
    if (tdmaTimer != nullptr) {
        cancelAndDelete(tdmaTimer);
    }
}