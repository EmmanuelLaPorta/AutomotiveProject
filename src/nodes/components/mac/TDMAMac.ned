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
        input upperIn[];    // Da sender apps
        output upperOut;    // A receiver app
        input lowerIn;      // Da link fisico
        output lowerOut;    // A link fisico
}
