#ifndef __AUTOMOTIVETDMANETWORK_RELAYUNIT_H_
#define __AUTOMOTIVETDMANETWORK_RELAYUNIT_H_

#include <omnetpp.h>

using namespace omnetpp;

class RelayUnit : public cSimpleModule
{
  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
};

#endif