// src/core/common/Constants.h
#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <omnetpp.h>

using namespace omnetpp;

namespace tdma {

// Network parameters
const uint64_t DATARATE = 1000000000; // 1 Gbps
const int MTU_BYTES = 1500;
const int ETHERNET_OVERHEAD = 38; // Header + IFG + CRC

// Time constants as inline functions to avoid global initialization
inline SimTime getSwitchDelay() { return SimTime(5, SIMTIME_US); }
inline SimTime getPropagationDelay() { return SimTime(10, SIMTIME_NS); }
inline SimTime getGuardTime() { return SimTime(1, SIMTIME_US); }
inline SimTime getIfgTime() { return SimTime(96, SIMTIME_NS); }
inline SimTime getHyperperiod() { return SimTime(100, SIMTIME_MS); }

// For backward compatibility with direct constant usage
#define SWITCH_DELAY getSwitchDelay()
#define PROPAGATION_DELAY getPropagationDelay()
#define GUARD_TIME getGuardTime()
#define IFG_TIME getIfgTime()
#define HYPERPERIOD getHyperperiod()

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