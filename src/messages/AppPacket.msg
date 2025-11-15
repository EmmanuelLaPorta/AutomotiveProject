// src/messages/AppPacket.msg
packet AppPacket {
    string appName;
    int sequenceNumber;
    simtime_t timestamp;
}