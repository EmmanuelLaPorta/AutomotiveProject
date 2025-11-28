// Scheduler TDMA - calcola tabella di scheduling offline
package tdma.core.scheduler;

simple TDMAScheduler
{
    parameters:
        double hyperperiod @unit(s);              // Durata ciclo scheduling (LCM periodi)
        double datarate @unit(bps);               // Bitrate link (default 1Gbps)
        double guardTime @unit(s) = default(1us); // Guard time tra slot
        double switchDelay @unit(s) = default(5us);      // Latenza store-and-forward switch
        double propagationDelay @unit(s) = default(10ns); // Ritardo propagazione cavo
        
        @display("i=block/cogwheel");
}