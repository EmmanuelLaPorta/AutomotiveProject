// src/nodes/components/applications/TDMASenderApp.ned
package tdma.nodes.components.applications;

simple TDMASenderApp {
    parameters:
        @display("i=block/app");
        string flowId = default("");
        string srcAddr = default("");
        string dstAddr = default("");
        int payloadSize @unit(B) = default(1500B);
        int burstSize = default(1);
        string tdmaSlots = default("") @mutable;
        double txDuration @unit(s) = default(0s) @mutable;
    
    gates:
        output out;
}