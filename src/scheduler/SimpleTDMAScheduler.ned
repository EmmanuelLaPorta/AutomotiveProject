// SimpleTDMAScheduler.ned
package automotivetdmanetwork.scheduler;

simple SimpleTDMAScheduler
{
    parameters:
        @display("i=block/control");
        double hyperperiod @unit(s) = default(0.1s);
        double datarate @unit(bps) = default(1Gbps);
        int ethernetOverhead @unit(B) = default(38B);
}