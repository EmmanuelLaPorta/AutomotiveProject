// src/nodes/components/applications/TDMAReceiverApp.ned
package tdma.nodes.components.applications;

simple TDMAReceiverApp {
    parameters:
        @display("i=block/app");
        string flowId = default("");
        
        @signal[e2eDelay](type=simtime_t);
        @signal[jitter](type=simtime_t);
        @signal[throughput](type=double);
        
        @statistic[e2eDelay](record=vector,stats,histogram);
        @statistic[jitter](record=vector,stats,histogram);
        @statistic[throughput](record=vector,stats);
    
    gates:
        input in;
}