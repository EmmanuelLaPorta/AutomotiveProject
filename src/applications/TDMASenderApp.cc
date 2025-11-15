// TDMASenderApp.cc
#include "TDMASenderApp.h"
#include "../messages/AppPackets_m.h"
#include "../messages/EthernetFrame_m.h"

Define_Module(TDMASenderApp);

void TDMASenderApp::initialize() {
    name = par("name").str();
    payloadSize = par("payloadSize");
    destAddr = par("destAddr").str();
    srcAddr = par("srcAddr").str();
    period = par("period");
    
    currentSlot = 0;
    totalPacketsSent = 0;
    
    // Parsing degli offset TDMA dalla stringa
    std::string offsetsStr = par("tdmaOffsets").stringValue();
    
    if (offsetsStr.empty()) {
        error("tdmaOffsets non configurato per %s", name.c_str());
    }
    
    std::stringstream ss(offsetsStr);
    std::string token;
    
    while (std::getline(ss, token, ',')) {
        if (!token.empty()) {
            txSlots.push_back(SimTime(std::stod(token), SIMTIME_S));
        }
    }
    
    EV << "=== TDMASenderApp [" << name << "] ===" << endl;
    EV << "Payload: " << payloadSize << " B" << endl;
    EV << "Periodo: " << period << " s" << endl;
    EV << "Slot TDMA configurati: " << txSlots.size() << endl;
    
    if (txSlots.size() < 3) {
        for (const auto& slot : txSlots) {
            EV << "  Slot: t=" << slot << " s" << endl;
        }
    }
    
    // Schedula trasmissioni per tutti gli slot
    for (size_t i = 0; i < txSlots.size(); i++) {
        cMessage *timer = new cMessage("SendPacket");
        timer->setKind(i + 1);  // Packet number (1-based)
        scheduleAt(txSlots[i], timer);
    }
}

void TDMASenderApp::handleMessage(cMessage *msg) {
    if (msg->isSelfMessage()) {
        int pktNumber = msg->getKind();
        sendPacket(pktNumber);
        delete msg;
    }
}

void TDMASenderApp::sendPacket(int pktNumber) {
    DataPacket *pkt = new DataPacket(name.c_str());
    pkt->setByteLength(payloadSize);
    pkt->setGenTime(simTime());
    pkt->setPktNumber(pktNumber);
    pkt->setBurstSize(1);  // Single packet per period
    
    EthTransmitReq *req = new EthTransmitReq();
    req->setSrc(srcAddr.c_str());
    req->setDst(destAddr.c_str());
    pkt->setControlInfo(req);
    
    send(pkt, "lowerLayerOut");
    totalPacketsSent++;
    
    EV_DEBUG << "[" << name << "] Pkt #" << pktNumber 
             << " inviato a t=" << simTime() << " s" << endl;
}

void TDMASenderApp::finish() {
    EV << "=== Sender " << name << " Statistics ===" << endl;
    EV << "Pacchetti inviati: " << totalPacketsSent << endl;
    
    recordScalar("totalPacketsSent", totalPacketsSent);
}