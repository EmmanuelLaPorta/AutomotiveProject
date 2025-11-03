package automotivetdmanetwork.switch;

simple EtherMAC {
    parameters:
    	@display("i=block/mac");
    	int datarate @unit(bps) = default(1Gbps);
    	object vlans = default([]);
    	
    	// ✅ CORRETTO: Signal per monitoraggio code
    	@signal[txQueueLength](type=long);
    	@signal[rxQueueLength](type=long);
    	
    	@statistic[txQueueLength](record=vector,stats,max);
    	@statistic[rxQueueLength](record=vector,stats,max);
    gates:
        input upperLayerIn;
        output upperLayerOut;
        input channelIn;
        output channelOut;
}