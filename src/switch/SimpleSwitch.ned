// SimpleSwitch.ned
package automotivetdmanetwork.switch;

simple SimpleSwitch
{
    parameters:
        @display("i=device/switch");
        double datarate @unit(bps) = default(1Gbps);
        double switchingDelay @unit(s) = default(5us);
        int numPorts = default(2);   // Numero di porte

        // Statistiche
        @signal[txQueueLength](type=long);
        @statistic[txQueueLength](record=vector,stats,max);

    gates:
        inout port[numPorts];
}