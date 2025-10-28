package automotivetdmanetwork.nodes;

import automotivetdmanetwork.applications.*;
import automotivetdmanetwork.encapsulation.EthEncap;
import automotivetdmanetwork.switch.EtherMAC;

module EndSystem
{
    parameters:
        int numSenders = default(0);
        int numReceivers = default(0);
        string macAddress;
        @display("i=device/device");
    gates:
        inout ethg;
    submodules:
        senderApp[numSenders]: TDMASenderApp {
            @display("p=100,50,row,80");
        }
        receiverApp[numReceivers]: TDMAReceiverApp {
            @display("p=300,50,row,80");
        }
        dispatcher: AppDispatcher {
            @display("p=200,150");
        }
        ethEncap: EthEncap {
            address = macAddress;
            @display("p=200,250");
        }
        mac: EtherMAC {
            @display("p=200,350");
        }
    connections:
        // Senders -> Dispatcher
        for i=0..numSenders-1 {
            senderApp[i].lowerLayerOut --> dispatcher.upperLayerIn++;
        }
        
        // Dispatcher -> Receivers
        for i=0..numReceivers-1 {
            dispatcher.upperLayerOut++ --> receiverApp[i].lowerLayerIn;
        }
        
        // Dispatcher <-> EthEncap
        dispatcher.lowerLayerOut --> ethEncap.upperLayerIn;
        dispatcher.lowerLayerIn <-- ethEncap.upperlayerOut;
        
        // EthEncap <-> MAC
        ethEncap.lowerLayerOut --> mac.upperLayerIn;
        ethEncap.lowerLayerIn <-- mac.upperLayerOut;
        
        // MAC <-> Canale esterno
        mac.channelOut --> ethg$o;
        mac.channelIn <-- ethg$i;
}