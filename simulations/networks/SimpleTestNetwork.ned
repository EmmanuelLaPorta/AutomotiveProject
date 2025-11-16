// simulations/networks/SimpleTestNetwork.ned
package networks;

import tdma.core.scheduler.TDMAScheduler;
import tdma.nodes.EndSystem;
import tdma.switch.TDMASwitch;
import ned.DatarateChannel;

network SimpleTestNetwork {
    parameters:
        @display("bgb=800,600");

    types:
        channel GigabitEthernet extends DatarateChannel {
            datarate = 1Gbps;
            delay = 10ns;
        }

    submodules:
        // Scheduler TDMA
        tdmaScheduler: TDMAScheduler {
            @display("p=400,50;i=block/control");
        }

        // Switches
        switch1: TDMASwitch {
            numPorts = 2;  // Solo 2 porte usate
            @display("p=300,300;i=device/switch");
        }

        switch2: TDMASwitch {
            numPorts = 3;  // Solo 3 porte usate
            @display("p=500,300;i=device/switch");
        }

        // End Systems
        LD1: EndSystem {
            numSenders = 1;
            numReceivers = 0;
            macAddress = "00:00:00:00:00:03";
            @display("p=150,300");
        }

        LD2: EndSystem {
            numSenders = 1;
            numReceivers = 0;
            macAddress = "00:00:00:00:00:0A";
            @display("p=500,150");
        }

        CU: EndSystem {
            numSenders = 0;
            numReceivers = 1;
            macAddress = "00:00:00:00:00:07";
            @display("p=650,300");
        }

    connections:
        // LD1 ↔ Switch1
        LD1.ethg <--> GigabitEthernet <--> switch1.port[0];

        // Switch1 ↔ Switch2
        switch1.port[1] <--> GigabitEthernet <--> switch2.port[0];

        // LD2 ↔ Switch2
        LD2.ethg <--> GigabitEthernet <--> switch2.port[1];

        // Switch2 ↔ CU
        switch2.port[2] <--> GigabitEthernet <--> CU.ethg;
}