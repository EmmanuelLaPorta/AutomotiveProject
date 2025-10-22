package rtn_01;

simple PeriodicTrafficGen
{
    parameters:
        volatile double period @unit(s) = default(0s);
        double startTime @unit(s) = default(0s);
        string name;
        int payloadSize @unit(B) = default(0B);
        int burstSize = default(1);
        string destAddr = default("");
        string srcAddr;
        @display("i=block/app2");
    gates:
        input lowerLayerIn;
        output lowerLayerOut;
}
