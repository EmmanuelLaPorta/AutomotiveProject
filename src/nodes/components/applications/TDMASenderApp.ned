package tdma.nodes.components.applications;

simple TDMASenderApp {
    parameters:
        @display("i=block/app");
        // Parametri Identificativi
        string flowId = default("") @mutable;
        string srcAddr = default("") @mutable;
        string dstAddr = default("") @mutable;           // MAC Address o "multicast"

        // Parametri per lo scheduler
        string dstNode = default("");                    // Nome del nodo destinazione (es. "CU") per routing unicast
        string destinations = default("");               // Lista nomi nodi (es. "S1,S2") per multicast
        double period @unit(s) = default(0.1s);          // Periodo di generazione

        // Parametri payload
        int payloadSize @unit(B) = default(1500B) @mutable;
        int burstSize = default(1) @mutable;             // Frammenti totali

        // Output dello scheduler
        string tdmaSlots = default("") @mutable;         // CSV offset in secondi
        double txDuration @unit(s) = default(0s) @mutable;
        double hyperperiod @unit(s) = default(0.2s) @mutable;

    gates:
        output out;
}