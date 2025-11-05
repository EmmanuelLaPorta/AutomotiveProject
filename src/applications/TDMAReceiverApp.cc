#include "TDMAReceiverApp.h"
#include "../messages/AppPackets_m.h"

Define_Module(TDMAReceiverApp);

void TDMAReceiverApp::initialize()
{
    name = par("name").str();
    
    // Inizializzazione per calcolo jitter
    lastDelay = 0;
    firstPacket = true;
    packetsReceived = 0;
}

void TDMAReceiverApp::handleMessage(cMessage *msg)
{
    DataPacket *pkt = check_and_cast<DataPacket *>(msg);
    
    // Filtra solo i pacchetti destinati a questa app
    if(strcmp(pkt->getName(), name.c_str()) != 0) {
        delete pkt;
        return;
    }

    packetsReceived++;
    
    EV_DEBUG << "Ricevuto pacchetto #" << pkt->getPktNumber()
            << "/" << pkt->getBurstSize() 
            << " (totale ricevuti: " << packetsReceived << ")" << endl;
    
    simtime_t delay = simTime() - pkt->getGenTime();

    // ✅ JITTER CORRETTO: Calcola la variazione di delay tra pacchetti consecutivi
    if(!firstPacket) {
        simtime_t jitter = fabs((delay - lastDelay).dbl());
        emit(registerSignal("jitter"), jitter);
        
        EV_DEBUG << "E2E Delay: " << delay << ", Last Delay: " << lastDelay 
                 << ", Jitter: " << jitter << endl;
    } else {
        EV_DEBUG << "Primo pacchetto, jitter non calcolato" << endl;
    }
    
    // Aggiorna per il prossimo pacchetto
    lastDelay = delay;
    firstPacket = false;

    // Emetti E2E Delay
    emit(registerSignal("E2EDelay"), delay);

    // Se è l'ultimo frammento del burst, emetti anche E2EBurstDelay
    if(pkt->getPktNumber() == pkt->getBurstSize()) {
        emit(registerSignal("E2EBurstDelay"), delay);
        EV_DEBUG << "Burst completo ricevuto, E2EBurstDelay: " << delay << endl;
    }

    delete pkt;
}

void TDMAReceiverApp::finish()
{
    EV << "=== Statistiche Receiver ===" << endl;
    EV << "Flow: " << name << endl;
    EV << "Pacchetti totali ricevuti: " << packetsReceived << endl;
    
    recordScalar("totalPacketsReceived", packetsReceived);
}