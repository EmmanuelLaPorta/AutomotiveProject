// src/core/scheduler/TDMAScheduler.ned
package tdma.core.scheduler;

simple TDMAScheduler {
    parameters:
        @display("i=block/control");
        double hyperperiod @unit(s) = default(100ms);
        double datarate @unit(bps) = default(1Gbps);
}