package automotivetdmanetwork.scheduler;

simple TDMAScheduler
{
    parameters:
        @display("i=block/control");
        double hyperperiod @unit(s) = default(0.1s);  // 100ms
        double datarate @unit(bps) = default(1Gbps);
        int ethernetOverhead @unit(B) = default(38B);  // 26 header + 12 IFG/CRC
}