package automotivetdmanetwork.applications;

simple TDMASenderApp
{
    parameters:
        double period @unit(s);
        volatile double tdmaOffset @unit(s) = default(0s);  // Calcolato dallo scheduler
        double fragmentTxTime @unit(s) = default(12.304us); // Tempo TX singolo frammento
        string name;
        int payloadSize @unit(B);
        int burstSize = default(1);
        string destAddr;
        string srcAddr;
        @display("i=block/app2");
    gates:
        output lowerLayerOut;
}