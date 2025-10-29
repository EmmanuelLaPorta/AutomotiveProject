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

        // TLM: Telematics (GPS/LTE)
        // FLOW 7: TLM → HU, CU (2 destinazioni multicast)
        TLM: EndSystem {
            macAddress = "00:00:00:00:00:01";
            numSenders = 2;    // AGGIORNATO: 2 sender per flow 7 (HU + CU)
            numReceivers = 0;
            @display("p=150,100");
        }

        // USS1: Ultrasonic Sensor 1 (parcheggio)
        // FLOW 3: USS1 → CU
        USS1: EndSystem {
            macAddress = "00:00:00:00:00:02";
            numSenders = 1;
            numReceivers = 0;
            @display("p=250,100");
        }

        // LD1: LiDAR frontale 1
        // FLOW 1: LD1 → CU
        LD1: EndSystem {
            macAddress = "00:00:00:00:00:03";
            numSenders = 1;
            numReceivers = 0;
            @display("p=350,100");
        }

        // FC: Front Camera (60 FPS)
        // FLOW 5: FC → HU (video RAW 178500B)
        FC: EndSystem {
            macAddress = "00:00:00:00:00:04";
            numSenders = 1;
            numReceivers = 0;
            @display("p=450,100");
        }

        // S1: Speaker 1
        // FLOW 2: Riceve da ME
        S1: EndSystem {
            macAddress = "00:00:00:00:00:05";
            numSenders = 0;
            numReceivers = 1;
            @display("p=150,400");
        }

        // HU: Head Unit (display principale)
        // Riceve da: CU (flow4), FC (flow5), TLM (flow7), RC (flow8)
        HU: EndSystem {
            macAddress = "00:00:00:00:00:06";
            numSenders = 0;
            numReceivers = 4;  // AGGIORNATO: CU, FC, TLM, RC
            @display("p=250,400");
        }

        // ===== NODI SWITCH 2 (Anteriore Destro) =====

        // CU: Control Unit (centralina principale)
        // FLOW 4: CU → HU (10500B frammentati)
        // Riceve da: LD1 (flow1), LD2 (flow1b), USS1-4 (flow3), TLM (flow7)
        CU: EndSystem {
            macAddress = "00:00:00:00:00:07";
            numSenders = 1;    // Flow 4: CU → HU
            numReceivers = 7;  // AGGIORNATO: LD1, LD2, USS1, USS2, USS3, USS4, TLM
            @display("p=800,100");
        }

        // S2: Speaker 2
        // FLOW 2: Riceve da ME
        S2: EndSystem {
            macAddress = "00:00:00:00:00:08";
            numSenders = 0;
            numReceivers = 1;
            @display("p=900,100");
        }

        // USS2: Ultrasonic Sensor 2
        // FLOW 3: USS2 → CU
        USS2: EndSystem {
            macAddress = "00:00:00:00:00:09";
            numSenders = 1;
            numReceivers = 0;
            @display("p=800,400");
        }

        // LD2: LiDAR frontale 2
        // FLOW 1b: LD2 → CU
        LD2: EndSystem {
            macAddress = "00:00:00:00:00:0A";
            numSenders = 1;
            numReceivers = 0;
            @display("p=900,400");
        }

        // ===== NODI SWITCH 3 (Posteriore Sinistro) =====

        // ME: Media Entertainment
        // FLOW 2: ME → S1, S2, S3, S4 (4 destinazioni audio)
        // FLOW 6: ME → RSE (video streaming 178500B)
        ME: EndSystem {
            macAddress = "00:00:00:00:00:0B";
            numSenders = 5;    // AGGIORNATO: 4 per flow2 (speaker) + 1 per flow6 (RSE)
            numReceivers = 0;
            @display("p=150,650");
        }

        // USS4: Ultrasonic Sensor 4
        // FLOW 3: USS4 → CU
        USS4: EndSystem {
            macAddress = "00:00:00:00:00:0C";
            numSenders = 1;
            numReceivers = 0;
            @display("p=250,650");
        }

        // S3: Speaker 3
        // FLOW 2: Riceve da ME
        S3: EndSystem {
            macAddress = "00:00:00:00:00:0D";
            numSenders = 0;
            numReceivers = 1;
            @display("p=350,650");
        }

        // RSE: Rear Seat Entertainment
        // FLOW 6: Riceve da ME (streaming video)
        RSE: EndSystem {
            macAddress = "00:00:00:00:00:0E";
            numSenders = 0;
            numReceivers = 1;
            @display("p=450,650");
        }

        // ===== NODI SWITCH 4 (Posteriore Destro) =====

        // RC: Rear Camera (30 FPS retrocamera)
        // FLOW 8: RC → HU (178000B)
        RC: EndSystem {
            macAddress = "00:00:00:00:00:0F";
            numSenders = 1;
            numReceivers = 0;
            @display("p=800,650");
        }

        // USS3: Ultrasonic Sensor 3
        // FLOW 3: USS3 → CU
        USS3: EndSystem {
            macAddress = "00:00:00:00:00:10";
            numSenders = 1;
            numReceivers = 0;
            @display("p=900,650");
        }

        // S4: Speaker 4
        // FLOW 2: Riceve da ME
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

