#include "TDMAReceiverApp.h"
#include "../messages/AppPackets_m.h"

Define_Module(TDMAReceiverApp);

void TDMAReceiverApp::initialize()
{
    name = par("name").str();
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

    // Emetti statistiche
    simsignal_t sig = registerSignal("E2EDelay");
    emit(sig, delay);

    if(pkt->getPktNumber() == pkt->getBurstSize()) {
        sig = registerSignal("E2EBurstDelay");
        emit(sig, delay);
    }

    delete pkt;
}