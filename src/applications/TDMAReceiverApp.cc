#include "TDMAReceiverApp.h"
#include "../messages/AppPackets_m.h"

Define_Module(TDMAReceiverApp);

void TDMAReceiverApp::initialize()
{
    name = par("name").str();
    
    // ✅ CORRETTO: Inizializzazione variabili per jitter
    lastDelay = 0;
    firstPacket = true;
}

void TDMAReceiverApp::handleMessage(cMessage *msg)
{
    DataPacket *pkt = check_and_cast<DataPacket *>(msg);
    
    // Filtra solo i pacchetti destinati a questa app
    if(strcmp(pkt->getName(), name.c_str()) != 0) {
        delete pkt;
        return;
    }

    EV_DEBUG << "Ricevuto pacchetto no. " << pkt->getPktNumber()
            << " di " << pkt->getBurstSize() << endl;
    
    simtime_t delay = simTime() - pkt->getGenTime();

    // ✅ CORRETTO: Calcolo jitter
    if(!firstPacket) {
        simtime_t jitter = fabs((delay - lastDelay).dbl());
        emit(registerSignal("jitter"), jitter);
    }
    
    // Aggiorna per il prossimo pacchetto
    lastDelay = delay;
    firstPacket = false;

    // Emetti statistiche E2E Delay
    emit(registerSignal("E2EDelay"), delay);

    // Se è l'ultimo frammento del burst, emetti anche E2EBurstDelay
    if(pkt->getPktNumber() == pkt->getBurstSize()) {
        emit(registerSignal("E2EBurstDelay"), delay);
    }

    delete pkt;
}