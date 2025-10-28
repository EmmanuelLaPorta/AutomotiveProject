package automotivetdmanetwork.applications;

simple TDMASenderApp
{
    parameters:
        double period @unit(s);
        double tdmaOffset @unit(s) = default(0s);  // Offset TDMA slot
        string name;
        int payloadSize @unit(B);
        int burstSize = default(1);
        string destAddr;
        string srcAddr;
        @display("i=block/app2");
    gates:
        output lowerLayerOut;
}