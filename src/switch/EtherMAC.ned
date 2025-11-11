package automotivetdmanetwork.switch;

simple EtherMAC {
    parameters:
        @display("i=block/mac");
        int datarate @unit(bps) = default(1Gbps);
        object vlans = default([]);
        
        // ✅ NUOVO: Supporto TDMA per switch
		bool enableTDMA = default(false) @mutable;
		string tdmaSlots = default("") @mutable;
        
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