// Frame TDMA con supporto frammentazione e multicast
cplusplus {{
#include <omnetpp.h>
using namespace omnetpp;
}}

packet TDMAFrame {
    string srcAddr;          // MAC sorgente
    string dstAddr;          // MAC destinazione (specifico o multicast)
    string flowId;           // Identificativo flusso
    int slotNumber;          // Indice slot nella tabella
    int fragmentNumber;      // Indice frammento (0-based)
    int totalFragments;      // Numero totale frammenti
    simtime_t txTime;        // Durata trasmissione
    simtime_t genTime;       // Timestamp generazione
    bool lastFragment;       // Flag ultimo frammento del burst
}
