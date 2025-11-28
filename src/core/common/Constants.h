/*
 * Costanti globali per simulazione TDMA automotive
 * Valori di rete Gigabit Ethernet secondo standard IEEE 802.3
 */
#ifndef TDMA_CONSTANTS_H
#define TDMA_CONSTANTS_H

#include <omnetpp.h>

namespace tdma {

// Parametri di rete fissi
const int MTU_BYTES = 1500;           // Maximum Transmission Unit Ethernet
const int ETHERNET_OVERHEAD = 38;     // Preamble(7) + SFD(1) + Header(14) + FCS(4) + IFG(12)
const double DATARATE = 1e9;          // 1 Gbps

// Inter-Frame Gap: 96 bit time @ 1Gbps = 96ns
inline omnetpp::simtime_t getIfgTime() { 
    return omnetpp::SimTime(96.0 / DATARATE); 
}

}

#endif