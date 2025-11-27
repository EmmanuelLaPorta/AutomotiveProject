// Switch Ethernet con priority queuing
package tdma.switch;

simple TDMASwitch {
    parameters:
        @display("i=device/switch");
        int numPorts = default(4);
        double switchingDelay @unit(s) = default(5us) @mutable;
        string macTableConfig = default("") @mutable;  // "MAC->port;port,..."

        @signal[queueLength](type=long);
        @statistic[queueLength](record=vector,stats,max);

    gates:
        inout port[numPorts];
}
