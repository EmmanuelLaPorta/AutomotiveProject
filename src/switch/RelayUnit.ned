package automotivetdmanetwork.switch;

simple RelayUnit {
    parameters:
    	@display("i=block/switch");
    gates:
        input portGatesIn[];
        output portGatesOut[];
}