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
    
    // FILTRO CRITICO: Ignora pacchetti di altri flussi
    if(strcmp(pkt->getName(), name.c_str()) != 0) {
        delete pkt;
        return;
    }

    packetsReceived++;
    
    simtime_t delay = simTime() - pkt->getGenTime();

    // JITTER CORRETTO: Solo tra pacchetti CONSECUTIVI dello STESSO flusso
    if(!firstPacket) {
        simtime_t jitter = fabs((delay - lastDelay).dbl());
        emit(registerSignal("jitter"), jitter);
        
        EV_DEBUG << "[" << name << "] Pkt #" << pkt->getPktNumber() 
                 << " - E2E: " << delay 
                 << ", Jitter: " << jitter << endl;
    } else {
        EV_DEBUG << "[" << name << "] Primo pacchetto, jitter=0" << endl;
        firstPacket = false;
    }
    
    // Aggiorna SEMPRE per il prossimo pacchetto
    lastDelay = delay;

    // Emetti E2E Delay
    emit(registerSignal("E2EDelay"), delay);

    // Se è l'ultimo frammento del burst, emetti anche E2EBurstDelay
    if(pkt->getPktNumber() == pkt->getBurstSize()) {
        emit(registerSignal("E2EBurstDelay"), delay);
        EV_DEBUG << "[" << name << "] Burst completo" << endl;
    }

    delete pkt;
}

void TDMAReceiverApp::finish()
{
    EV << "=== Statistiche " << name << " ===" << endl;
    EV << "Pacchetti ricevuti: " << packetsReceived << endl;
    
    recordScalar("totalPacketsReceived", packetsReceived);
}