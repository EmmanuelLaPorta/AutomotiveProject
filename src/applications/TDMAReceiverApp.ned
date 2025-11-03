package automotivetdmanetwork.applications;

simple TDMAReceiverApp
{
    parameters:
        string name;
        @display("i=block/app");
        
        // ✅ CORRETTO: Tutti i signal necessari
        @signal[E2EDelay](type=simtime_t);
        @signal[E2EBurstDelay](type=simtime_t);
        @signal[jitter](type=simtime_t);
        
        @statistic[E2EDelay](record=vector,stats,histogram);
        @statistic[E2EBurstDelay](record=vector,stats,histogram);
        @statistic[jitter](record=vector,stats,histogram);
    gates:
        input lowerLayerIn;
}