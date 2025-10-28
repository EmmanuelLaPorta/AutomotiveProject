package automotivetdmanetwork.applications;

simple TDMAReceiverApp
{
    parameters:
        string name;
        @display("i=block/app");
        @signal[E2EDelay](type=simtime_t);
        @signal[E2EBurstDelay](type=simtime_t);
        @statistic[E2EDelay](record=vector,stats);
        @statistic[E2EBurstDelay](record=vector,stats);
    gates:
        input lowerLayerIn;
}