#ifndef __AUTOMOTIVETDMANETWORK_RELAYUNIT_H_
#define __AUTOMOTIVETDMANETWORK_RELAYUNIT_H_

#include <omnetpp.h>
#include <map>
#include <string>
#include <vector> // Aggiunto per std::vector
#include "../messages/EthernetFrame_m.h"
#include "../common/WatchVector.h"

using namespace omnetpp;

class RelayUnit : public cSimpleModule
{
  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

  private:
    // Tabella di forwarding STATICA: mappa un MAC di destinazione a un vettore di porte di uscita.
    // Questo permette il forwarding deterministico sia unicast (vettore con 1 porta)
    // sia multicast (vettore con N porte).
    std::map<std::string, std::vector<int>> staticForwardingTable;

    // Funzione helper per popolare la tabella statica all'inizializzazione.
    void populateStaticForwardingTable();
};

#endif
