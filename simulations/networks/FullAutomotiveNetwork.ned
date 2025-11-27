// Rete automotive TDMA: 4 switch, 18 end-node
package networks;

import tdma.core.scheduler.TDMAScheduler;
import tdma.nodes.EndSystem;
import tdma.switch.TDMASwitch;
import ned.DatarateChannel;

network FullAutomotiveNetwork {
    parameters:
        @display("bgb=1200,800");

    types:
        channel GigabitEthernet extends DatarateChannel {
            datarate = 1Gbps;
            delay = 10ns;
        }

    submodules:
        tdmaScheduler: TDMAScheduler {
            @display("p=600,50;i=block/control");
        }

        // Switch (4 zone veicolo)
        switch1: TDMASwitch { numPorts = 8; @display("p=300,300"); }  // Anteriore SX
        switch2: TDMASwitch { numPorts = 6; @display("p=700,300"); }  // Anteriore DX
        switch3: TDMASwitch { numPorts = 6; @display("p=300,500"); }  // Posteriore SX
        switch4: TDMASwitch { numPorts = 6; @display("p=700,500"); }  // Posteriore DX

        // LiDAR frontali
        LD1: EndSystem { numSenders = 1; numReceivers = 0; macAddress = "00:00:00:00:00:03"; @display("p=200,200"); }
        LD2: EndSystem { numSenders = 1; numReceivers = 0; macAddress = "00:00:00:00:00:0A"; @display("p=800,200"); }

        // Control Unit e Head Unit
        CU: EndSystem { numSenders = 1; numReceivers = 1; macAddress = "00:00:00:00:00:07"; @display("p=800,400"); }
        HU: EndSystem { numSenders = 0; numReceivers = 1; macAddress = "00:00:00:00:00:06"; @display("p=200,300"); }

        // Media Entertainment
        ME: EndSystem { numSenders = 2; numReceivers = 0; macAddress = "00:00:00:00:00:0B"; @display("p=200,600"); }

        // Speaker (4)
        S1: EndSystem { numSenders = 0; numReceivers = 1; macAddress = "00:00:00:00:00:05"; @display("p=200,400"); }
        S2: EndSystem { numSenders = 0; numReceivers = 1; macAddress = "00:00:00:00:00:08"; @display("p=800,300"); }
        S3: EndSystem { numSenders = 0; numReceivers = 1; macAddress = "00:00:00:00:00:0D"; @display("p=200,500"); }
        S4: EndSystem { numSenders = 0; numReceivers = 1; macAddress = "00:00:00:00:00:11"; @display("p=800,600"); }

        // Sensori ultrasuoni (4)
        US1: EndSystem { numSenders = 1; numReceivers = 0; macAddress = "00:00:00:00:00:02"; @display("p=150,200"); }
        US2: EndSystem { numSenders = 1; numReceivers = 0; macAddress = "00:00:00:00:00:09"; @display("p=900,200"); }
        US3: EndSystem { numSenders = 1; numReceivers = 0; macAddress = "00:00:00:00:00:10"; @display("p=800,700"); }
        US4: EndSystem { numSenders = 1; numReceivers = 0; macAddress = "00:00:00:00:00:0C"; @display("p=150,700"); }

        // Camere
        CM1: EndSystem { numSenders = 1; numReceivers = 0; macAddress = "00:00:00:00:00:04"; @display("p=100,150"); }
        RC: EndSystem { numSenders = 1; numReceivers = 0; macAddress = "00:00:00:00:00:0F"; @display("p=800,700"); }

        // Telematics
        TLM: EndSystem { numSenders = 2; numReceivers = 0; macAddress = "00:00:00:00:00:01"; @display("p=100,250"); }

        // Rear Seat Entertainment
        RS1: EndSystem { numSenders = 0; numReceivers = 1; macAddress = "00:00:00:00:00:12"; @display("p=900,600"); }
        RS2: EndSystem { numSenders = 0; numReceivers = 1; macAddress = "00:00:00:00:00:0E"; @display("p=200,600"); }

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
