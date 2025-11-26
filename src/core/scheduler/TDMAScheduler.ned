// src/core/scheduler/TDMAScheduler.ned
package tdma.core.scheduler;

simple TDMAScheduler
{
    parameters:
        double hyperperiod @unit(s);
        double datarate @unit(bps);
        double guardTime @unit(s) = default(1us); // <--- QUESTA MANCAVA
        
        @display("i=block/cogwheel");
}