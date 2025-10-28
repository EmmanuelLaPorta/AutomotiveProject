#ifndef __AUTOMOTIVETDMANETWORK_APPDISPATCHER_H_
#define __AUTOMOTIVETDMANETWORK_APPDISPATCHER_H_

#include <omnetpp.h>

using namespace omnetpp;

class AppDispatcher : public cSimpleModule
{
  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
};

#endif