#include "RelayUnit.h"
#include "../messages/EthernetFrame_m.h"
#include <string>
#include <vector>

Define_Module(RelayUnit);

void RelayUnit::initialize()
{
    // Sostituisce la MAC table dinamica con una tabella di forwarding STATICA
    // per un comportamento deterministico, essenziale in una rete TDMA.
    WATCH_MAP(staticForwardingTable);
    populateStaticForwardingTable();
}

void RelayUnit::handleMessage(cMessage *msg)
{
    EthernetFrame *frame = check_and_cast<EthernetFrame *>(msg);
    int arrivalPort = msg->getArrivalGate()->getIndex();
    const char* destMAC = frame->getDst();

    auto it = staticForwardingTable.find(destMAC);
    if (it != staticForwardingTable.end()) {
        // Destinazione (unicast o multicast) trovata nella tabella statica
        const std::vector<int>& destinationPorts = it->second;
        
        // Duplica il frame per ogni porta di destinazione tranne quella di arrivo
        int frameCopies = 0;
        for (int destPort : destinationPorts) {
            if (destPort != arrivalPort) {
                EV_DEBUG << "Forwarding statico: MAC " << destMAC << " -> porta " << destPort << endl;
                EthernetFrame *copy = frame->dup();
                send(copy, "portGatesOut", destPort);
                frameCopies++;
            }
        }

        // L'originale può essere cancellato
        delete frame;

    } else {
        // In una rete statica, ogni destinazione deve essere nota.
        // Scarta il pacchetto se la destinazione non è nella tabella.
        EV_WARN << "Destinazione sconosciuta in tabella statica: " << destMAC << ". Pacchetto scartato." << endl;
        delete frame;
    }
}

void RelayUnit::populateStaticForwardingTable()
{
    std::string switchName = getParentModule()->getName();
    EV << "Inizializzazione tabella di forwarding statica per " << switchName << endl;

    // =======================================================================================
    // DEFINIZIONE INDIRIZZI MAC
    // Questi devono corrispondere ESATTAMENTE a quelli in network_topology.ned e omnetpp.ini
    // =======================================================================================
    const char* HU_MAC = "00:00:00:00:00:06";
    const char* CU_MAC = "00:00:00:00:00:07";
    const char* S1_MAC = "00:00:00:00:00:05";
    const char* S2_MAC = "00:00:00:00:00:08";
    const char* S3_MAC = "00:00:00:00:00:0D";
    const char* S4_MAC = "00:00:00:00:00:11";
    const char* RS1_MAC = "00:00:00:00:00:12";
    const char* RS2_MAC = "00:00:00:00:00:0E";

    // Indirizzi MAC Multicast (devono corrispondere a quelli usati in TDMASenderApp e TDMAScheduler)
    // Questi indirizzi sono scelti per rappresentare i flussi multicast.
    const char* MCAST_AUDIO_FLOW2 = "01:00:5E:00:00:02"; // Flow 2: ME -> S1,S2,S3,S4
    const char* MCAST_VIDEO_FLOW6 = "01:00:5E:00:00:06"; // Flow 6: ME -> RS1,RS2
    const char* MCAST_TELE_FLOW7 = "01:00:5E:00:00:07";  // Flow 7: TLM -> HU,CU

    if (switchName == "switch1") {
        // Porte: 0=TLM, 1=US1, 2=LD1, 3=CM1, 4=S1, 5=HU, 6=switch2, 7=switch3
        staticForwardingTable[HU_MAC] = {5};
        staticForwardingTable[CU_MAC] = {6}; // via switch2
        staticForwardingTable[S1_MAC] = {4};
        // Unicast forwarding per pacchetti che arrivano da altri switch
        staticForwardingTable[S2_MAC] = {6}; // via switch2
        staticForwardingTable[S3_MAC] = {7}; // via switch3
        staticForwardingTable[S4_MAC] = {7}; // via switch3
        staticForwardingTable[RS1_MAC] = {7}; // via switch3
        staticForwardingTable[RS2_MAC] = {7}; // via switch3
        // Multicast
        staticForwardingTable[MCAST_AUDIO_FLOW2] = {4, 6, 7}; // S1, to switch2 (for S2), to switch3 (for S3, S4)
        staticForwardingTable[MCAST_VIDEO_FLOW6] = {7};       // to switch3 (for RS1, RS2)
        staticForwardingTable[MCAST_TELE_FLOW7] = {5, 6};     // HU, to switch2 (for CU)
    }
    else if (switchName == "switch2") {
        // Porte: 0=CU, 1=S2, 2=US2, 3=LD2, 4=switch1, 5=switch4
        staticForwardingTable[HU_MAC] = {4}; // via switch1
        staticForwardingTable[CU_MAC] = {0};
        staticForwardingTable[S1_MAC] = {4}; // via switch1
        staticForwardingTable[S2_MAC] = {1};
        staticForwardingTable[S3_MAC] = {4}; // via switch1
        staticForwardingTable[S4_MAC] = {5}; // via switch4
        staticForwardingTable[RS1_MAC] = {5}; // via switch4
        staticForwardingTable[RS2_MAC] = {4}; // via switch1
        // Multicast
        staticForwardingTable[MCAST_AUDIO_FLOW2] = {1, 4, 5}; // S2, to switch1 (for S1), to switch4 (for S4)
        staticForwardingTable[MCAST_VIDEO_FLOW6] = {5};       // to switch4 (for RS1, RS2)
        staticForwardingTable[MCAST_TELE_FLOW7] = {0, 4};     // CU, to switch1 (for HU)
    }
    else if (switchName == "switch3") {
        // Porte: 0=ME, 1=US4, 2=S3, 3=RS2, 4=switch1, 5=switch4
        staticForwardingTable[HU_MAC] = {4}; // via switch1
        staticForwardingTable[CU_MAC] = {4}; // via switch1
        staticForwardingTable[S1_MAC] = {4}; // via switch1
        staticForwardingTable[S2_MAC] = {4}; // via switch1
        staticForwardingTable[S3_MAC] = {2};
        staticForwardingTable[S4_MAC] = {5}; // via switch4
        staticForwardingTable[RS1_MAC] = {5}; // via switch4
        staticForwardingTable[RS2_MAC] = {3};
        // Multicast
        staticForwardingTable[MCAST_AUDIO_FLOW2] = {2, 4, 5}; // S3, to switch1 (for S1,S2), to switch4 (for S4)
        staticForwardingTable[MCAST_VIDEO_FLOW6] = {3, 5};    // RS2, to switch4 (for RS1)
        staticForwardingTable[MCAST_TELE_FLOW7] = {4};        // to switch1 (for HU, CU)
    }
    else if (switchName == "switch4") {
        // Porte: 0=RC, 1=US3, 2=S4, 3=switch2, 4=switch3, 5=RS1
        staticForwardingTable[HU_MAC] = {3}; // via switch2
        staticForwardingTable[CU_MAC] = {3}; // via switch2
        staticForwardingTable[S1_MAC] = {4}; // via switch3
        staticForwardingTable[S2_MAC] = {3}; // via switch2
        staticForwardingTable[S3_MAC] = {4}; // via switch3
        staticForwardingTable[S4_MAC] = {2};
        staticForwardingTable[RS1_MAC] = {5};
        staticForwardingTable[RS2_MAC] = {4}; // via switch3
        // Multicast
        staticForwardingTable[MCAST_AUDIO_FLOW2] = {2, 3, 4}; // S4, to switch2 (for S2), to switch3 (for S1, S3)
        staticForwardingTable[MCAST_VIDEO_FLOW6] = {4, 5};    // to switch3 (for RS2), RS1
        staticForwardingTable[MCAST_TELE_FLOW7] = {3};        // to switch2 (for HU, CU)
    }
}

void RelayUnit::finish()
{
    // La tabella è statica, non c'è stato finale da mostrare se non la sua configurazione iniziale.
    EV << "=== Tabella di Forwarding Statica Utilizzata (" << getParentModule()->getName() << ") ===" << endl;
    for (const auto &entry : staticForwardingTable) {
        std::stringstream ports;
        for(int port : entry.second) {
            ports << port << " ";
        }
        EV << "  " << entry.first << " -> porte " << ports.str() << endl;
    }
}