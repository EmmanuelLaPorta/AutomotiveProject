// Nodo terminale: combina sender/receiver apps con MAC layer
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

    connections allowunconnected:
        for i=0..numSenders-1 {
             senderApp[i].out --> mac.upperIn++;
        }

        mac.upperOut --> receiverApp[0].in if numReceivers > 0;

        mac.lowerOut --> ethg$o;
        mac.lowerIn <-- ethg$i;
}
