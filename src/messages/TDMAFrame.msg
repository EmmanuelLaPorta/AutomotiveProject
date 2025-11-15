// src/messages/TDMAFrame.msg
cplusplus {{
#include <omnetpp.h>
using namespace omnetpp;
}}

packet TDMAFrame {
    string srcAddr;
    string dstAddr;
    string flowId;
    int slotNumber;
    int fragmentNumber;
    int totalFragments;
    simtime_t txTime;
    simtime_t genTime;
    bool lastFragment;
}