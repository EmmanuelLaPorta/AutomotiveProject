#include "RelayUnit.h"

Define_Module(RelayUnit);

void RelayUnit::initialize()
{
    // TODO - Generated method body
}

void RelayUnit::handleMessage(cMessage *msg)
{
    for(int i=0; i < gate("portGatesIn")->size(); i++) {
        if(i != msg->getArrivalGate()->getIndex()) {
            send(msg->dup(), gate("portGatesOut", i));
        }
    }
    delete msg;
}
