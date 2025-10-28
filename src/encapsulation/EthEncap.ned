package automotivetdmanetwork.encapsulation;

simple EthEncap
{
    parameters:
    	@display("i=block/layer");
    	string address; // Indirizzo MAC della porta
    gates:
        output lowerLayerOut;
        input lowerLayerIn;
        output upperlayerOut;
        input upperLayerIn;
}