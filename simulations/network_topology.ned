import automotivetdmanetwork.nodes.EndSystem;
import automotivetdmanetwork.switch.EtherSwitch;
import ned.DatarateChannel;

@license(LGPL);

channel EthernetChannel extends DatarateChannel
{
    datarate = 1Gbps;
    delay = 0s;
    ber = 0;
}

network AutomotiveTDMANetwork
{
    parameters:
        @display("bgb=1200,800");
        
    submodules:
        // ===== SWITCH 1 - Anteriore Sinistro =====
        switch1: EtherSwitch {
            portCount = 8; // 6 nodi + 2 switch (switch2 e switch3)
            @display("p=300,250;i=device/switch");
        }
        
        // ===== SWITCH 2 - Anteriore Destro =====
        switch2: EtherSwitch {
            portCount = 6; // 4 nodi + 2 switch (switch1 e switch4)
            @display("p=700,250;i=device/switch");
        }
        
        // ===== SWITCH 3 - Posteriore Sinistro =====
        switch3: EtherSwitch {
            portCount = 6; // 4 nodi + 2 switch (switch1 e switch4)
            @display("p=300,550;i=device/switch");
        }
        
        // ===== SWITCH 4 - Posteriore Destro =====
        switch4: EtherSwitch {
            portCount = 5; // 3 nodi + 2 switch (switch2 e switch3)
            @display("p=700,550;i=device/switch");
        }
        
        // ===== NODI SWITCH 1 (Anteriore Sinistro) =====
        TLM: EndSystem {
            macAddress = "00:00:00:00:00:01";
            numSenders = 1;
            numReceivers = 0;
            @display("p=150,100");
        }
        
        USS1: EndSystem {
            macAddress = "00:00:00:00:00:02";
            numSenders = 1;
            numReceivers = 0;
            @display("p=250,100");
        }
        
        LD1: EndSystem {
            macAddress = "00:00:00:00:00:03";
            numSenders = 1;
            numReceivers = 0;
            @display("p=350,100");
        }
        
        FC: EndSystem {
            macAddress = "00:00:00:00:00:04";
            numSenders = 1;
            numReceivers = 0;
            @display("p=450,100");
        }
        
        S1: EndSystem {
            macAddress = "00:00:00:00:00:05";
            numSenders = 0;
            numReceivers = 1;
            @display("p=150,400");
        }
        
        HU: EndSystem {
            macAddress = "00:00:00:00:00:06";
            numSenders = 0;
            numReceivers = 4; // Riceve da: CU, FC, TLM, RC
            @display("p=250,400");
        }
        
        // ===== NODI SWITCH 2 (Anteriore Destro) =====
        CU: EndSystem {
            macAddress = "00:00:00:00:00:07";
            numSenders = 1;
            numReceivers = 6; // Riceve da: LD1, USS1, USS2, USS3, USS4, TLM
            @display("p=800,100");
        }
        
        S2: EndSystem {
            macAddress = "00:00:00:00:00:08";
            numSenders = 0;
            numReceivers = 1;
            @display("p=900,100");
        }
        
        USS2: EndSystem {
            macAddress = "00:00:00:00:00:09";
            numSenders = 1;
            numReceivers = 0;
            @display("p=800,400");
        }
        
        LD2: EndSystem {
            macAddress = "00:00:00:00:00:0A";
            numSenders = 1;
            numReceivers = 0;
            @display("p=900,400");
        }
        
        // ===== NODI SWITCH 3 (Posteriore Sinistro) =====
        ME: EndSystem {
            macAddress = "00:00:00:00:00:0B";
            numSenders = 2; // Invia a: S1,S2,S3,S4 (flusso 2) e RSE (flusso 6)
            numReceivers = 0;
            @display("p=150,650");
        }
        
        USS4: EndSystem {
            macAddress = "00:00:00:00:00:0C";
            numSenders = 1;
            numReceivers = 0;
            @display("p=250,650");
        }
        
        S3: EndSystem {
            macAddress = "00:00:00:00:00:0D";
            numSenders = 0;
            numReceivers = 1;
            @display("p=350,650");
        }
        
        RSE: EndSystem {
            macAddress = "00:00:00:00:00:0E";
            numSenders = 0;
            numReceivers = 1;
            @display("p=450,650");
        }
        
        // ===== NODI SWITCH 4 (Posteriore Destro) =====
        RC: EndSystem {
            macAddress = "00:00:00:00:00:0F";
            numSenders = 1;
            numReceivers = 0;
            @display("p=800,650");
        }
        
        USS3: EndSystem {
            macAddress = "00:00:00:00:00:10";
            numSenders = 1;
            numReceivers = 0;
            @display("p=900,650");
        }
        
        S4: EndSystem {
            macAddress = "00:00:00:00:00:11";
            numSenders = 0;
            numReceivers = 1;
            @display("p=1000,650");
        }
        
    connections:
        // ===== COLLEGAMENTI SWITCH 1 =====
        // Nodi su Switch 1
        TLM.ethg$o --> EthernetChannel --> switch1.channelIn[0];
        TLM.ethg$i <-- EthernetChannel <-- switch1.channelOut[0];
        
        USS1.ethg$o --> EthernetChannel --> switch1.channelIn[1];
        USS1.ethg$i <-- EthernetChannel <-- switch1.channelOut[1];
        
        LD1.ethg$o --> EthernetChannel --> switch1.channelIn[2];
        LD1.ethg$i <-- EthernetChannel <-- switch1.channelOut[2];
        
        FC.ethg$o --> EthernetChannel --> switch1.channelIn[3];
        FC.ethg$i <-- EthernetChannel <-- switch1.channelOut[3];
        
        S1.ethg$o --> EthernetChannel --> switch1.channelIn[4];
        S1.ethg$i <-- EthernetChannel <-- switch1.channelOut[4];
        
        HU.ethg$o --> EthernetChannel --> switch1.channelIn[5];
        HU.ethg$i <-- EthernetChannel <-- switch1.channelOut[5];
        
        // Collegamenti inter-switch da Switch 1
        switch1.channelOut[6] --> EthernetChannel --> switch2.channelIn[4];
        switch1.channelIn[6] <-- EthernetChannel <-- switch2.channelOut[4];
        
        switch1.channelOut[7] --> EthernetChannel --> switch3.channelIn[4];
        switch1.channelIn[7] <-- EthernetChannel <-- switch3.channelOut[4];
        
        // ===== COLLEGAMENTI SWITCH 2 =====
        // Nodi su Switch 2
        CU.ethg$o --> EthernetChannel --> switch2.channelIn[0];
        CU.ethg$i <-- EthernetChannel <-- switch2.channelOut[0];
        
        S2.ethg$o --> EthernetChannel --> switch2.channelIn[1];
        S2.ethg$i <-- EthernetChannel <-- switch2.channelOut[1];
        
        USS2.ethg$o --> EthernetChannel --> switch2.channelIn[2];
        USS2.ethg$i <-- EthernetChannel <-- switch2.channelOut[2];
        
        LD2.ethg$o --> EthernetChannel --> switch2.channelIn[3];
        LD2.ethg$i <-- EthernetChannel <-- switch2.channelOut[3];
        
        // Collegamento inter-switch da Switch 2 a Switch 4
        switch2.channelOut[5] --> EthernetChannel --> switch4.channelIn[3];
        switch2.channelIn[5] <-- EthernetChannel <-- switch4.channelOut[3];
        
        // ===== COLLEGAMENTI SWITCH 3 =====
        // Nodi su Switch 3
        ME.ethg$o --> EthernetChannel --> switch3.channelIn[0];
        ME.ethg$i <-- EthernetChannel <-- switch3.channelOut[0];
        
        USS4.ethg$o --> EthernetChannel --> switch3.channelIn[1];
        USS4.ethg$i <-- EthernetChannel <-- switch3.channelOut[1];
        
        S3.ethg$o --> EthernetChannel --> switch3.channelIn[2];
        S3.ethg$i <-- EthernetChannel <-- switch3.channelOut[2];
        
        RSE.ethg$o --> EthernetChannel --> switch3.channelIn[3];
        RSE.ethg$i <-- EthernetChannel <-- switch3.channelOut[3];
        
        // Collegamento inter-switch da Switch 3 a Switch 4
        switch3.channelOut[5] --> EthernetChannel --> switch4.channelIn[4];
        switch3.channelIn[5] <-- EthernetChannel <-- switch4.channelOut[4];
        
        // ===== COLLEGAMENTI SWITCH 4 =====
        // Nodi su Switch 4
        RC.ethg$o --> EthernetChannel --> switch4.channelIn[0];
        RC.ethg$i <-- EthernetChannel <-- switch4.channelOut[0];
        
        USS3.ethg$o --> EthernetChannel --> switch4.channelIn[1];
        USS3.ethg$i <-- EthernetChannel <-- switch4.channelOut[1];
        
        S4.ethg$o --> EthernetChannel --> switch4.channelIn[2];
        S4.ethg$i <-- EthernetChannel <-- switch4.channelOut[2];
}