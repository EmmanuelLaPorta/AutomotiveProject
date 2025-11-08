#ifndef __AUTOMOTIVETDMANETWORK_RELAYUNIT_H_
#define __AUTOMOTIVETDMANETWORK_RELAYUNIT_H_

#include <omnetpp.h>
#include <map>
#include <string>
#include "../messages/EthernetFrame_m.h"

using namespace omnetpp;

class RelayUnit : public cSimpleModule
{
  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    
    // ✅ NUOVO: MAC Address Learning Table
    // Mappa: MAC address → porta dello switch
    std::map<std::string, int> macTable;
    
    // Helper per forwarding broadcast
    void forwardBroadcast(EthernetFrame *frame, int arrivalPort);
};

#endif
