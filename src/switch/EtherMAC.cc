#include "EtherMAC.h"
#include "../messages/EthernetFrame_m.h"
#include <sstream>

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
    
    isTDMAEnabled = par("enableTDMA").boolValue();
    tdmaTimer = nullptr;
    currentSlotIndex = 0;
    isInTDMASlot = false;
    
    if (isTDMAEnabled) {
        std::string slotsStr = par("tdmaSlots").stringValue();
        
        if (!slotsStr.empty()) {
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
            
            if (!tdmaSlots.empty()) {
                tdmaTimer = new cMessage("TDMASlot");
                scheduleAt(tdmaSlots[0].offset, tdmaTimer);
            }
        }
    }
    
    emit(registerSignal("txQueueLength"), 0);
    emit(registerSignal("rxQueueLength"), 0);
}

bool EtherMAC::canTransmitNow(cPacket *pkt)
{
    if (!isTDMAEnabled) {
        return true;
    }
    
    // CRITICO: Se non siamo in uno slot TDMA valido, BLOCCA
    if (!isInTDMASlot) {
        return false;
    }
    
    if (currentSlotIndex >= tdmaSlots.size()) {
        return false;
    }
    
    EthernetFrame *frame = dynamic_cast<EthernetFrame *>(pkt);
    if (frame == nullptr) return false;
    
    cPacket *encapPkt = frame->getEncapsulatedPacket();
    if (encapPkt == nullptr) return false;
    
    const TDMASlot &currentSlot = tdmaSlots[currentSlotIndex];
    
    if (strcmp(encapPkt->getName(), currentSlot.flowName.c_str()) == 0) {
        return true;
    }
    
    return false;
}

void EtherMAC::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage() && strcmp(msg->getName(), "TDMASlot") == 0) {
        // Apri la finestra TDMA
        isInTDMASlot = true;

        EV_DEBUG << "TDMA Slot aperto a t=" << simTime()
                 << " per flow " << tdmaSlots[currentSlotIndex].flowName << endl;

        // Prova a trasmettere se c'è qualcosa in coda
        if (txstate == TX_STATE_IDLE && txqueue.getLength() > 0) {
            cPacket *pkt = check_and_cast<cPacket*>(txqueue.front());

            if (canTransmitNow(pkt)) {
                startTransmission();
            }
        }

        // --- INIZIO MODIFICA: Durata dello slot dinamica ---
        simtime_t slotDuration;
        const simtime_t guardTime = SimTime(2, SIMTIME_US); // 2µs di tempo di guardia
        size_t nextSlotIndex = currentSlotIndex + 1;

        if (nextSlotIndex < tdmaSlots.size()) {
            // Calcola la durata fino al prossimo slot, meno il tempo di guardia
            slotDuration = (tdmaSlots[nextSlotIndex].offset - simTime()) - guardTime;
        } else {
            // Per l'ultimo slot, usa una durata di default (non ci sono slot successivi)
            slotDuration = SimTime(1, SIMTIME_MS); // Esempio: 1ms
        }

        // Assicurati che la durata non sia negativa
        if (slotDuration < 0) {
            slotDuration = 0;
            EV_WARN << "Configurazione TDMA errata: slot si sovrappongono o guardTime troppo lungo." << endl;
        }
        
        // Schedula la chiusura dello slot
        cMessage *closeSlot = new cMessage("CloseSlot");
        scheduleAt(simTime() + slotDuration, closeSlot);
        // --- FINE MODIFICA ---

        return;
    }

    if (msg->isSelfMessage() && strcmp(msg->getName(), "CloseSlot") == 0) {
        delete msg;

        // Chiudi la finestra TDMA
        isInTDMASlot = false;

        // Avanza allo slot successivo
        currentSlotIndex++;

        // --- INIZIO MODIFICA: Correzione Memory Leak ---
        if (currentSlotIndex < tdmaSlots.size()) {
            // NON creare un nuovo timer, riutilizza quello esistente.
            // RIMOSSA: tdmaTimer = new cMessage("TDMASlot");
            scheduleAt(tdmaSlots[currentSlotIndex].offset, tdmaTimer);
        }
        // --- FINE MODIFICA ---

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

        if(txstate == TX_STATE_IDLE) {
            if (!isTDMAEnabled || canTransmitNow(pkt)) {
                startTransmission();
            } else {
                tdmaBlockedPackets++;
            }
        }
        return;
    }

    if(rxstate != RX_STATE_IDLE) {
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

    for(size_t i=0; i<vlans.size(); i++) {
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
    
    if (tdmaTimer != nullptr && tdmaTimer->isScheduled()) {
        cancelAndDelete(tdmaTimer);
        tdmaTimer = nullptr;
    }
}