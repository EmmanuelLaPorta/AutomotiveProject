// src/nodes/components/applications/TDMASenderApp.ned
package tdma.nodes.components.applications;

simple TDMASenderApp {
    parameters:
        @display("i=block/app");
        string flowId = default("") @mutable;
        string srcAddr = default("") @mutable;
        string dstAddr = default("") @mutable;
        int payloadSize @unit(B) = default(1500B) @mutable;
        int burstSize = default(1) @mutable;
        string tdmaSlots = default("") @mutable;
        double txDuration @unit(s) = default(0s) @mutable;

    gates:
        output out;
}