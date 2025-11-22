// src/nodes/components/applications/TDMAReceiverApp.ned
package tdma.nodes.components.applications;

simple TDMAReceiverApp {
    parameters:
        @display("i=block/app");
        string flowId = default("") @mutable;

    gates:
        input in;
}