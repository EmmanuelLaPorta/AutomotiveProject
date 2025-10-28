#include "AppDispatcher.h"

Define_Module(AppDispatcher);

void AppDispatcher::initialize()
{
    // TODO - Generated method body
}

void AppDispatcher::handleMessage(cMessage *msg)
{
    /*
     * Riceve un pacchetto da un'applicazione,
     * lo inoltra a lowerLayerOut.
     *
     * Riceve un pacchetto dalla rete, lo inoltra
     * a tutti gli upperLayerOut. I gate superiori sono
     * un vettore di gate.
     *
     * Inoltrare ad ogni gate superiore solo il duplicato.
     */
}
