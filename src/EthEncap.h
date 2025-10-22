#ifndef __RTN_01_ETHENCAP_H_
#define __RTN_01_ETHENCAP_H_

#include <omnetpp.h>

using namespace omnetpp;
using namespace std;

class EthEncap : public cSimpleModule
{
  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;

    string address;
};

#endif
