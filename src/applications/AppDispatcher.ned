package automotivetdmanetwork.applications;

simple AppDispatcher
{
    parameters:
    	@display("i=block/classifier");
    gates:
        output upperLayerOut[];  
        input upperLayerIn[];
        output lowerLayerOut;
        input lowerLayerIn;
}