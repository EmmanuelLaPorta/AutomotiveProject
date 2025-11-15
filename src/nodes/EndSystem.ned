// src/nodes/EndSystem.ned
package tdma.nodes;

import tdma.nodes.components.applications.TDMASenderApp;
import tdma.nodes.components.applications.TDMAReceiverApp;
import tdma.nodes.components.mac.TDMAMac;

module EndSystem {
    parameters:
        @display("i=device/pc");
        int numSenders = default(0);
        int numReceivers = default(0);
        string macAddress = default("");
    
    gates:
        inout ethg;
    
    submodules:
        senderApp[numSenders]: TDMASenderApp {
            @display("p=100,50,row,80");
        }
        
        receiverApp[numReceivers]: TDMAReceiverApp {
            @display("p=300,50,row,80");
        }
        
        mac: TDMAMac {
            macAddress = parent.macAddress;
            @display("p=200,200");
        }
    
    connections:
        // Senders to MAC
        for i=0..numSenders-1 {
            senderApp[i].out --> mac.upperIn;
        }
        
        // MAC to Receivers
        mac.upperOut --> receiverApp[0].in if numReceivers > 0;
        
        // MAC to Physical
        mac.lowerOut --> ethg$o;
        mac.lowerIn <-- ethg$i;
}