/*
 * Costanti globali per simulazione TDMA automotive
 * Valori di rete Gigabit Ethernet secondo standard IEEE 802.3
 */
#ifndef TDMA_CONSTANTS_H
#define TDMA_CONSTANTS_H

#include <omnetpp.h>

namespace tdma {

// Livelli di priorita per scheduling EDF (servono come tie-breaker nel caso in cui in EDF c'è la stessa priorità)
// 0 = massima priorita (safety-critical), 7 = minima (best-effort)
const int PRIO_CRITICAL_SAFETY = 0;
const int PRIO_CONTROL = 3;
const int PRIO_INFOTAINMENT = 6;
const int PRIO_BEST_EFFORT = 7;

// Parametri di rete
const int MTU_BYTES = 1500;           // Maximum Transmission Unit Ethernet
const int ETHERNET_OVERHEAD = 38;     // Preamble(7) + SFD(1) + Header(14) + FCS(4) + IFG(12)
const double DATARATE = 1e9;          // 1 Gbps

// Timing configurabili a runtime
inline double GLOBAL_GUARD_TIME = 1e-6;    // Guard time tra slot (1us default)
inline double SWITCH_DELAY = 5e-6;          // Latenza store-and-forward switch
inline double PROPAGATION_DELAY = 0.1e-6;   // Ritardo propagazione cavo (~10cm)

inline void setGuardTime(double time) { GLOBAL_GUARD_TIME = time; }
inline double getGuardTime() { return GLOBAL_GUARD_TIME; }
inline double getSwitchDelay() { return SWITCH_DELAY; }
inline double getPropagationDelay() { return PROPAGATION_DELAY; }

// Inter-Frame Gap: 96 bit time @ 1Gbps = 96ns
inline simtime_t getIfgTime() { return omnetpp::SimTime(96.0 / DATARATE); }

}

#endif
