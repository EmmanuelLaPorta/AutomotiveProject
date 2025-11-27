// Receiver TDMA con statistiche flow
package tdma.nodes.components.applications;

simple TDMAReceiverApp {
    parameters:
        @display("i=block/app");
        string flowId = default("") @mutable;  // Vuoto = accetta tutti

    gates:
        input in;
}
