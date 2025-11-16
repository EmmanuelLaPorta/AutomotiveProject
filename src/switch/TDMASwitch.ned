// src/switch/TDMASwitch.ned
package tdma.switch;

simple TDMASwitch {
    parameters:
        @display("i=device/switch");
        int numPorts = default(4);
        double switchingDelay @unit(s) = default(5us);
        string macTableConfig = default("") @mutable;  // <-- AGGIUNTO @mutable
        
        @signal[queueLength](type=long);
        @statistic[queueLength](record=vector,stats,max);
    
    gates:
        inout port[numPorts];
}