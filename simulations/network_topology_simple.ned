// Topologia semplificata per Flusso 1
// LD1 → Switch1 → Switch2 → CU
// LD2 → Switch2 → CU

import automotivetdmanetwork.nodes.EndSystem;
import automotivetdmanetwork.switch.SimpleSwitch;
import automotivetdmanetwork.scheduler.SimpleTDMAScheduler;
import ned.DatarateChannel;

@license(LGPL);

channel EthernetChannel extends DatarateChannel
{
    datarate = 1Gbps;
    delay = 10ns;
    ber = 0;
}

network SimpleTDMANetwork
{
    parameters:
        @display("bgb=800,400");

    submodules:
        // Scheduler TDMA
        scheduler: SimpleTDMAScheduler {
            @display("p=400,50;i=block/control");
        }

        // ✅ Switch 1 - CON MAC TABLE CONFIGURATA
        // Porta 0: LD1 (locale)
        // Porta 1: Switch2 (routing verso CU e LD2)
        switch1: SimpleSwitch {
            numPorts = 2;
            datarate = 1Gbps;
            switchingDelay = 5us;
            
            // ✅ CONFIGURAZIONE MAC TABLE SEMPLIFICATA:
            // LD1 locale sulla porta 0
            // CU raggiungibile via switch2 sulla porta 1
            macTableConfig = "00:00:00:00:00:03->0, 00:00:00:00:00:07->1";
            
            @display("p=250,200;i=device/switch");
        }

        // ✅ Switch 2 - CON MAC TABLE CONFIGURATA  
        // Porta 0: LD2 (locale)
        // Porta 1: CU (locale)
        // Porta 2: Switch1 (routing verso LD1)
        switch2: SimpleSwitch {
            numPorts = 3;
            datarate = 1Gbps;
            switchingDelay = 5us;
            
            // ✅ CONFIGURAZIONE MAC TABLE SEMPLIFICATA:
            // LD2 locale sulla porta 0
            // CU locale sulla porta 1
            // LD1 raggiungibile via switch1 sulla porta 2
            macTableConfig = "00:00:00:00:00:04->0, 00:00:00:00:00:07->1, 00:00:00:00:00:03->2";
            
            @display("p=550,200;i=device/switch");
        }

        // LD1: LiDAR frontale (FLOW 1 sender)
        LD1: EndSystem {
            macAddress = "00:00:00:00:00:03";
            numSenders = 1;
            numReceivers = 0;
            @display("p=100,200");
        }

        // LD2: LiDAR frontale (FLOW 1 sender)
        LD2: EndSystem {
            macAddress = "00:00:00:00:00:04";
            numSenders = 1;
            numReceivers = 0;
            @display("p=550,100");
        }

        // CU: Control Unit (FLOW 1 receiver)
        CU: EndSystem {
            macAddress = "00:00:00:00:00:07";
            numSenders = 0;
            numReceivers = 1;
            @display("p=700,200");
        }

    connections:
        // LD1 → Switch1 (porta 0)
        LD1.ethg$o --> EthernetChannel --> switch1.port$i[0];
        LD1.ethg$i <-- EthernetChannel <-- switch1.port$o[0];

        // LD2 → Switch2 (porta 0)
        LD2.ethg$o --> EthernetChannel --> switch2.port$i[0];
        LD2.ethg$i <-- EthernetChannel <-- switch2.port$o[0];

        // Switch1 ↔ Switch2
        // Switch1 porta 1 ↔ Switch2 porta 2
        switch1.port$o[1] --> EthernetChannel --> switch2.port$i[2];
        switch1.port$i[1] <-- EthernetChannel <-- switch2.port$o[2];

        // CU → Switch2 (porta 1)
        switch2.port$o[1] --> EthernetChannel --> CU.ethg$i;
        switch2.port$i[1] <-- EthernetChannel <-- CU.ethg$o;
}