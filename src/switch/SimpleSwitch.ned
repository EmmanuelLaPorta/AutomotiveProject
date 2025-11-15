// SimpleSwitch.ned - VERSIONE MODULARE
package automotivetdmanetwork.switch;

simple SimpleSwitch
{
    parameters:
        @display("i=device/switch");
        double datarate @unit(bps) = default(1Gbps);
        double switchingDelay @unit(s) = default(5us);
        int numPorts = default(2);
        
        // ✅ NUOVO: Configurazione MAC Table da parametro
        // Formato: "MAC1->port1, MAC2->port2, MAC3->port3"
        // Esempio: "00:00:00:00:00:03->0, 00:00:00:00:00:07->1"
        string macTableConfig = default("");

        // Statistiche
        @signal[txQueueLength](type=long);
        @statistic[txQueueLength](record=vector,stats,max);

    gates:
        inout port[numPorts];
}