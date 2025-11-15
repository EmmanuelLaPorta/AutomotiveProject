// src/core/common/Constants.h
#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <omnetpp.h>

using omnetpp::const_simtime_t;

namespace tdma {

// Network parameters
const uint64_t DATARATE = 1000000000; // 1 Gbps
const int MTU_BYTES = 1500;
const int ETHERNET_OVERHEAD = 38; // Header + IFG + CRC
const const_simtime_t SWITCH_DELAY = SimTime(5, SIMTIME_US);
const const_simtime_t PROPAGATION_DELAY = SimTime(10, SIMTIME_NS);
const const_simtime_t GUARD_TIME = SimTime(1, SIMTIME_US);
const const_simtime_t IFG_TIME = SimTime(96, SIMTIME_NS);

// TDMA parameters
const const_simtime_t HYPERPERIOD = SimTime(100, SIMTIME_MS);

// Flow priorities (lower = higher priority)
enum FlowPriority {
    PRIO_CRITICAL_SAFETY = 1,  // LiDAR
    PRIO_CONTROL = 2,          // Control messages
    PRIO_REALTIME = 3,         // Video streaming
    PRIO_INFOTAINMENT = 4,     // Audio, entertainment
    PRIO_BEST_EFFORT = 5       // Parking sensors
};

}

#endif