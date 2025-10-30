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

    EV << "=== TDMASenderApp Inizializzato ===" << endl;
    EV << "Nome: " << name << endl;
    EV << "Periodo: " << period << endl;
    EV << "TDMA Offset: " << tdmaOffset << endl;
    EV << "Fragment TX Time: " << fragmentTxTime << endl;
    EV << "Payload Size: " << payloadSize << " B" << endl;
    EV << "Burst Size: " << burstSize << " frammenti" << endl;

    // Schedule primo invio con offset TDMA
    if(tdmaOffset >= 0) {
        cMessage *timer = new cMessage("TxTimer");
        scheduleAt(tdmaOffset, timer);
        EV << "Primo invio schedulato a t=" << tdmaOffset << endl;
    }
}

void TDMASenderApp::handleMessage(cMessage *msg)
{
    if(msg->isSelfMessage()) {
        if(strcmp(msg->getName(), "TxTimer") == 0) {
            // Timer periodico: invia il burst di frammenti
            transmitPacket();
            
            // Schedula il prossimo burst nel prossimo periodo
            scheduleAt(simTime() + period, msg);
            return;
        }

        if(strcmp(msg->getName(), "FragmentTimer") == 0) {
            // Timer per singolo frammento
            int fragmentNumber = msg->getKind();  // Numero frammento memorizzato in kind
            sendFragment(fragmentNumber);
            delete msg;
            return;
        }

        error("Arrivato un self message non previsto: %s", msg->getName());
    }

    // Questo modulo invia solo, non riceve
    error("TDMASenderApp non dovrebbe ricevere messaggi dalla rete");
}

void TDMASenderApp::transmitPacket() 
{
    EV << "=== Inizio trasmissione burst ===" << endl;
    EV << "Tempo: " << simTime() << endl;
    EV << "Burst #" << (++currentBurstNumber) << endl;
    EV << "Frammenti da inviare: " << burstSize << endl;

    // ✅ SOLUZIONE FRAMMENTAZIONE DISTRIBUITA NEL TEMPO
    // Invece di inviare tutti i frammenti in un loop (SBAGLIATO),
    // scheduliamo ogni frammento con un offset temporale
    
    for(int i = 0; i < burstSize; i++) {
        // Calcola quando inviare questo frammento
        // Frammento 0: t = ora
        // Frammento 1: t = ora + fragmentTxTime
        // Frammento 2: t = ora + 2*fragmentTxTime
        // ...
        simtime_t fragmentOffset = i * fragmentTxTime;
        
        // Crea un timer per questo frammento
        cMessage *fragmentTimer = new cMessage("FragmentTimer");
        fragmentTimer->setKind(i + 1);  // Memorizza il numero del frammento (1-based)
        
        // Schedula l'invio con l'offset calcolato
        scheduleAt(simTime() + fragmentOffset, fragmentTimer);
        
        EV << "  Frammento " << (i+1) << "/" << burstSize 
           << " schedulato a t=" << (simTime() + fragmentOffset) << endl;
    }
}

void TDMASenderApp::sendFragment(int fragmentNumber)
{
    EV << "Invio frammento " << fragmentNumber << "/" << burstSize 
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

    // INVIA IL SINGOLO FRAMMENTO
    send(pkt, "lowerLayerOut");
    
    EV << "  Frammento inviato con successo" << endl;
}