packet EthernetFrame {
    string dst;
    string src;
    int etherType;
    byteLength = 26;
}

packet EthernetQFrame extends EthernetFrame {
    int vlanid;
    int pcp;
    byteLength = 30;
}

message EthTransmitReq {
    string dst;
    string src;
}