// Topologia semplificata per Flusso 1: LD1 → Switch1 → Switch2 → CU

import automotivetdmanetwork.nodes.EndSystem;
import automotivetdmanetwork.switch.SimpleSwitch;
import automotivetdmanetwork.scheduler.SimpleTDMAScheduler;
import ned.DatarateChannel;

@license(LGPL);

channel EthernetChannel extends DatarateChannel
{
    datarate = 1Gbps;
    delay = 10ns;  // Propagation delay
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

        // Switch 1 (Anteriore Sinistro)
        switch1: SimpleSwitch {
            parameters:
                numPorts = 2;
                datarate = 1Gbps;
                switchingDelay = 5us;
                @display("p=250,200;i=device/switch");
        }

        // Switch 2 (Anteriore Destro)
        switch2: SimpleSwitch {
            parameters:
                numPorts = 3;
                datarate = 1Gbps;
                switchingDelay = 5us;
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
            @display("p=500,150");
        }

        // CU: Control Unit (FLOW 1 receiver)
        CU: EndSystem {
            macAddress = "00:00:00:00:00:07";
            numSenders = 0;
            numReceivers = 1;
            @display("p=700,200");
        }

    connections:
        // LD1 → Switch1
        LD1.ethg$o --> EthernetChannel --> switch1.port$i[0];
        LD1.ethg$i <-- EthernetChannel <-- switch1.port$o[0];

 		// LD2 → Switch2
        LD2.ethg$o --> EthernetChannel --> switch2.port$i[0];
        LD2.ethg$i <-- EthernetChannel <-- switch2.port$o[0];

        // Switch1 ↔ Switch2
        switch1.port$o[1] --> EthernetChannel --> switch2.port$i[1];
        switch1.port$i[1] <-- EthernetChannel <-- switch2.port$o[1];

        // Switch2 → CU
        switch2.port$o[2] --> EthernetChannel --> CU.ethg$i;
        switch2.port$i[2] <-- EthernetChannel <-- CU.ethg$o;
}