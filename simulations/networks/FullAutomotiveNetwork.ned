// Rete automotive TDMA: 4 switch, 18 end-node
package networks;

import tdma.core.scheduler.TDMAScheduler;
import tdma.nodes.EndSystem;
import tdma.switch.TDMASwitch;
import ned.DatarateChannel;

network FullAutomotiveNetwork
{
    parameters:
        @display("bgb=1200,800");

    types:
        channel GigabitEthernet extends DatarateChannel
        {
            datarate = 1Gbps;
            delay = 10ns;
        }

    submodules:
        tdmaScheduler: TDMAScheduler {
            @display("p=600,50;i=block/control");
        }

        // Switch (4 zone veicolo)
        switch1: TDMASwitch {
            numPorts = 8;
            @display("p=262,486");
        }  // Anteriore SX
        switch2: TDMASwitch {
            numPorts = 6;
            @display("p=262,203");
        }  // Anteriore DX
        switch3: TDMASwitch {
            numPorts = 6;
            @display("p=957,486");
        }  // Posteriore SX
        switch4: TDMASwitch {
            numPorts = 6;
            @display("p=957,203");
        }  // Posteriore DX

        // LiDAR frontali
        LD1: EndSystem {
            numSenders = 1;
            numReceivers = 0;
            macAddress = "00:00:00:00:00:03";
            @display("p=103,487");
        }
        LD2: EndSystem {
            numSenders = 1;
            numReceivers = 0;
            macAddress = "00:00:00:00:00:0A";
            @display("p=103,80");
        }

        // Control Unit e Head Unit
        CU: EndSystem {
            numSenders = 1;
            numReceivers = 1;
            macAddress = "00:00:00:00:00:07";
            @display("p=320,317");
        }
        HU: EndSystem {
            numSenders = 0;
            numReceivers = 1;
            macAddress = "00:00:00:00:00:06";
            @display("p=542,317");
        }

        // Media Entertainment
        ME: EndSystem {
            numSenders = 2;
            numReceivers = 0;
            macAddress = "00:00:00:00:00:0B";
            @display("p=846,563");
        }

        // Speaker (4)
        S1: EndSystem {
            numSenders = 0;
            numReceivers = 1;
            macAddress = "00:00:00:00:00:05";
            @display("p=405,415");
        }
        S2: EndSystem {
            numSenders = 0;
            numReceivers = 1;
            macAddress = "00:00:00:00:00:08";
            @display("p=405,256");
        }
        S3: EndSystem {
            numSenders = 0;
            numReceivers = 1;
            macAddress = "00:00:00:00:00:0D";
            @display("p=846,415");
        }
        S4: EndSystem {
            numSenders = 0;
            numReceivers = 1;
            macAddress = "00:00:00:00:00:11";
            @display("p=846,256");
        }

        // Sensori ultrasuoni (4)
        US1: EndSystem {
            numSenders = 1;
            numReceivers = 0;
            macAddress = "00:00:00:00:00:02";
            @display("p=103,645");
        }
        US2: EndSystem {
            numSenders = 1;
            numReceivers = 0;
            macAddress = "00:00:00:00:00:09";
            @display("p=103,203");
        }
        US3: EndSystem {
            numSenders = 1;
            numReceivers = 0;
            macAddress = "00:00:00:00:00:10";
            @display("p=1066,203");
        }
        US4: EndSystem {
            numSenders = 1;
            numReceivers = 0;
            macAddress = "00:00:00:00:00:0C";
            @display("p=1045,626");
        }

        // Camere
        CM1: EndSystem {
            numSenders = 1;
            numReceivers = 0;
            macAddress = "00:00:00:00:00:04";
            @display("p=122,379");
        }
        RC: EndSystem {
            numSenders = 1;
            numReceivers = 0;
            macAddress = "00:00:00:00:00:0F";
            @display("p=1013,301");
        }

        // Telematics
        TLM: EndSystem {
            numSenders = 2;
            numReceivers = 0;
            macAddress = "00:00:00:00:00:01";
            @display("p=262,626");
        }

        // Rear Seat Entertainment
        RS1: EndSystem {
            numSenders = 0;
            numReceivers = 1;
            macAddress = "00:00:00:00:00:12";
            @display("p=722,256");
        }
        RS2: EndSystem {
            numSenders = 0;
            numReceivers = 1;
            macAddress = "00:00:00:00:00:0E";
            @display("p=722,402");
        }

    connections:
        // Switch 1
        S1.ethg <--> GigabitEthernet <--> switch1.port[0];
        LD1.ethg <--> GigabitEthernet <--> switch1.port[1];
        switch1.port[2] <--> GigabitEthernet <--> switch2.port[0];
        switch1.port[3] <--> GigabitEthernet <--> switch3.port[1];
        HU.ethg <--> GigabitEthernet <--> switch1.port[4];
        US1.ethg <--> GigabitEthernet <--> switch1.port[5];
        CM1.ethg <--> GigabitEthernet <--> switch1.port[6];
        TLM.ethg <--> GigabitEthernet <--> switch1.port[7];

        // Switch 2
        S2.ethg <--> GigabitEthernet <--> switch2.port[1];
        LD2.ethg <--> GigabitEthernet <--> switch2.port[2];
        CU.ethg <--> GigabitEthernet <--> switch2.port[3];
        switch2.port[4] <--> GigabitEthernet <--> switch4.port[0];
        US2.ethg <--> GigabitEthernet <--> switch2.port[5];

        // Switch 3
        ME.ethg <--> GigabitEthernet <--> switch3.port[0];
        S3.ethg <--> GigabitEthernet <--> switch3.port[2];
        switch3.port[3] <--> GigabitEthernet <--> switch4.port[1];
        US4.ethg <--> GigabitEthernet <--> switch3.port[4];
        RS2.ethg <--> GigabitEthernet <--> switch3.port[5];

        // Switch 4
        S4.ethg <--> GigabitEthernet <--> switch4.port[2];
        US3.ethg <--> GigabitEthernet <--> switch4.port[3];
        RC.ethg <--> GigabitEthernet <--> switch4.port[4];
        RS1.ethg <--> GigabitEthernet <--> switch4.port[5];
}
