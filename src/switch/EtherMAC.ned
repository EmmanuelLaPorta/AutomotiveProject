package automotivetdmanetwork.switch;

simple EtherMAC {
    parameters:
    	@display("i=block/mac");
    	int datarate @unit(bps) = default(1Gbps);
    	object vlans = default([]);
    gates:
        input upperLayerIn;
        output upperLayerOut;
        input channelIn;
        output channelOut;
}