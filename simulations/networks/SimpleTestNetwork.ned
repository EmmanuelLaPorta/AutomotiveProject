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
            numPorts = 4;
            @display("p=300,300;i=device/switch");
        }

        switch2: TDMASwitch {
            numPorts = 4;
            @display("p=500,300;i=device/switch");
        }

        // End Systems
        LD1: EndSystem {
            @display("p=150,300");
        }

        LD2: EndSystem {
            @display("p=500,150");
        }

        CU: EndSystem {
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