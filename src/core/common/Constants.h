// src/core/common/Constants.h
#ifndef TDMA_CONSTANTS_H
#define TDMA_CONSTANTS_H

#include <omnetpp.h>

namespace tdma {
    // Priority Levels
    const int PRIO_CRITICAL_SAFETY = 0;
    const int PRIO_CONTROL = 3;
    const int PRIO_INFOTAINMENT = 6;
    const int PRIO_BEST_EFFORT = 7;

    // Network Constants
    const int MTU_BYTES = 1500;
    const int ETHERNET_OVERHEAD = 38; // Preamble + SFD + Header + FCS + IFG
    const double DATARATE = 1e9; // 1 Gbps

    // Variabili globali gestite (usiamo inline per poterle definire nell'header)
    inline double GLOBAL_GUARD_TIME = 1e-6; // Default 1us
    inline double SWITCH_DELAY = 5e-6;      // Default 5us
    inline double PROPAGATION_DELAY = 0.1e-6; // Default molto basso (cavo corto)

    // Setters & Getters
    inline void setGuardTime(double time) { GLOBAL_GUARD_TIME = time; }
    inline double getGuardTime() { return GLOBAL_GUARD_TIME; } // <--- RISOLVE ERRORE NAMESPACE
    
    inline double getSwitchDelay() { return SWITCH_DELAY; }
    inline double getPropagationDelay() { return PROPAGATION_DELAY; }
    
    inline simtime_t getIfgTime() { return omnetpp::SimTime(96.0 / DATARATE); } // Inter-frame gap (96 bits)
}

#endif