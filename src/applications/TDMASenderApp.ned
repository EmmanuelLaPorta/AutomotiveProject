package automotivetdmanetwork.applications;

simple TDMASenderApp
{
    parameters:
        double period @unit(s);
        double tdmaOffset @unit(s) = default(0s) @mutable;
        double fragmentTxTime @unit(s) = default(0s) @mutable;  // Sarà impostato dallo scheduler
        string name;
        int payloadSize @unit(B);
        int burstSize = default(1);
        string destAddr;
        string srcAddr;
        @display("i=block/app2");
    gates:
        output lowerLayerOut;
}