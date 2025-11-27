package tdma.nodes.components.applications;

simple TDMASenderApp {
    parameters:
        @display("i=block/app");
        string flowId = default("") @mutable;
        string srcAddr = default("") @mutable;
        string dstAddr = default("") @mutable;           // MAC o "multicast"
        int payloadSize @unit(B) = default(1500B) @mutable;
        int burstSize = default(1) @mutable;             // Frammenti totali
        int numDestinations = default(1) @mutable;       // Per multicast
        string tdmaSlots = default("") @mutable;         // CSV offset in secondi
        double txDuration @unit(s) = default(0s) @mutable;

    gates:
        output out;
}
