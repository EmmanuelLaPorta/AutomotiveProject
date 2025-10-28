package automotivetdmanetwork.switch;

module EtherSwitch
{
    parameters:
        int portCount;
        @display("bgb=824,326;i=device/modem");
    gates:
        input channelIn[portCount];
        output channelOut[portCount];
    submodules:
        eth[portCount]: EtherMAC {
            @display("p=61,266,r,30");
        }
        relayUnit: RelayUnit {
            @display("p=378,75");
        }
    connections:
        for i=0..portCount-1 {
            eth[i].upperLayerIn <-- relayUnit.portGatesOut++;
            eth[i].upperLayerOut --> relayUnit.portGatesIn++;
            eth[i].channelIn <-- channelIn[i];
            eth[i].channelOut --> channelOut[i];
        }
}