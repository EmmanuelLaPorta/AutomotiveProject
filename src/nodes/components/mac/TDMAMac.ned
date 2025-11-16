// src/nodes/components/mac/TDMAMac.ned
package tdma.nodes.components.mac;

simple TDMAMac {
    parameters:
        @display("i=block/mac");
        double datarate @unit(bps) = default(1Gbps) @mutable;
        string macAddress = default("") @mutable;

        @signal[txQueueLength](type=long);
        @signal[rxQueueLength](type=long);

        @statistic[txQueueLength](record=vector,stats,max);
        @statistic[rxQueueLength](record=vector,stats,max);

    gates:
        input upperIn;
        output upperOut;
        input lowerIn;
        output lowerOut;
}