// Implementazione scheduler
#include "TDMAScheduler.h"
#include "../common/Constants.h" 
#include <algorithm>
#include <sstream>
#include <set>
#include <map>
#include <queue>
#include <iostream>

Define_Module(TDMAScheduler);

// Prenotazione di un link per un intervallo temporale
struct LinkReservation {
    simtime_t start;
    simtime_t end;
};

// Tabella globale delle prenotazioni link
static std::map<std::string, std::vector<LinkReservation>> linkTable;

// Job da schedulare: un frammento di un flusso in una specifica istanza
struct Job {
    std::string flowId;
    std::string srcNode;
    simtime_t releaseTime;
    simtime_t deadline;
    simtime_t txDuration;
    int fragmentIndex;
    std::vector<std::string> destinations;
};

// Verifica disponibilita link in [start, start+duration+guard]
static bool isLinkFree(const std::string& linkId, simtime_t start, simtime_t duration, simtime_t guard) {
    simtime_t reqEnd = start + duration + guard;
    if (linkTable.find(linkId) == linkTable.end()) return true;
    for (const auto& res : linkTable[linkId]) {
        if (start < res.end && res.start < reqEnd) return false;
    }
    return true;
}

static void reserveLink(const std::string& linkId, simtime_t start, simtime_t duration, simtime_t guard) {
    linkTable[linkId].push_back({start, start + duration + guard});
}


void TDMAScheduler::initialize() {
    hyperperiod = par("hyperperiod");
    datarate = par("datarate").doubleValue();
    guardTime = par("guardTime").doubleValue();
    switchDelay = par("switchDelay").doubleValue();
    propagationDelay = par("propagationDelay").doubleValue();

    // Reset strutture
    flows.clear();
    schedule.clear();
    linkTable.clear();
    adjacency.clear();
    nodeMacAddress.clear();
    pathCache.clear();

    std::cout << "TDMA SCHEDULER: Inizializzazione..." << std::endl;

    // Discovery topologia dalla rete NED
    discoverTopology();
    
    // Leggo configurazione flussi
    discoverFlowsFromNetwork();
    
    // Calcolo tabella di scheduling
    generateOptimizedSchedule();
    
    // Distribuisco configurazione
    configureSenders();
    configureSwitches();
    
    std::cout << "TDMA SCHEDULER: Inizializzazione completata" << std::endl;
}

void TDMAScheduler::discoverTopology() {
    cModule *network = getParentModule();
    
    std::cout << "TDMA SCHEDULER: Discovery topologia..." << std::endl;
    
    // Raccogli tutti i nodi e i loro MAC address
    for (cModule::SubmoduleIterator it(network); !it.end(); ++it) {
        cModule *node = *it;
        std::string nodeName = node->getName();
        
        // Salta lo scheduler stesso
        if (nodeName == "tdmaScheduler") continue;
        
        // Raccogli MAC address dagli EndSystem
        if (node->hasPar("macAddress")) {
            std::string mac = node->par("macAddress").stringValue();
            if (!mac.empty()) {
                nodeMacAddress[nodeName] = mac;
                EV << "Nodo " << nodeName << " MAC: " << mac << endl;
            }
        }
        
        // Inizializza entry nel grafo
        adjacency[nodeName] = {};
    }
    
    // Lambda per risalire al modulo di rete (serve a saltare i moduli interni)
    auto getNetworkModule = [network](cModule *mod) -> cModule* {
        if (!mod) return nullptr;
        // Risali finche' il parent non e' la rete
        while (mod->getParentModule() != network && mod->getParentModule() != nullptr) {
            mod = mod->getParentModule();
        }
        return (mod->getParentModule() == network) ? mod : nullptr;
    };
    
    // Scopri le connessioni navigando le gate
    for (cModule::SubmoduleIterator it(network); !it.end(); ++it) {
        cModule *node = *it;
        std::string nodeName = node->getName();
        
        if (nodeName == "tdmaScheduler") continue;
        
        // Controlla se e' uno switch (lo riconosco perche' ha gate array "port")
        bool isSwitch = (nodeName.find("switch") != std::string::npos);
        
        if (isSwitch) {
            // Switch: itera sulle porte
            int numPorts = node->par("numPorts");
            for (int p = 0; p < numPorts; p++) {
                cGate *outGate = node->gate("port$o", p);
                if (outGate && outGate->isConnected()) {
                    cGate *nextGate = outGate->getNextGate();
                    if (nextGate) {
                        // Potrebbe essere un channel, naviga fino al modulo
                        cGate *destGate = outGate->getPathEndGate();
                        if (destGate) {
                            cModule *rawNeighbor = destGate->getOwnerModule();
                            cModule *neighbor = getNetworkModule(rawNeighbor);
                            if (neighbor && neighbor != node) {
                                std::string neighborName = neighbor->getName();
                                adjacency[nodeName].push_back({neighborName, p});
                                EV << "Connessione: " << nodeName << "[" << p << "] -> " << neighborName << endl;
                            }
                        }
                    }
                }
            }
        } else {
            // EndSystem: ha una singola gate "ethg"
            cGate *outGate = node->gate("ethg$o");
            if (outGate && outGate->isConnected()) {
                cGate *destGate = outGate->getPathEndGate();
                if (destGate) {
                    cModule *rawNeighbor = destGate->getOwnerModule();
                    cModule *neighbor = getNetworkModule(rawNeighbor);
                    if (neighbor && neighbor != node) {
                        std::string neighborName = neighbor->getName();
                        adjacency[nodeName].push_back({neighborName, 0});
                        EV << "Connessione: " << nodeName << " -> " << neighborName << endl;
                    }
                }
            }
        }
    }
    
    std::cout << "TDMA SCHEDULER: Topologia scoperta - " 
              << adjacency.size() << " nodi, " 
              << nodeMacAddress.size() << " MAC address" << std::endl;
}

void TDMAScheduler::discoverFlowsFromNetwork() {
    cModule *network = getParentModule(); 
    
    std::cout << "TDMA SCHEDULER: Acquisizione flussi..." << std::endl;
    
    // Itera su tutti i nodi della rete
    for (cModule::SubmoduleIterator it(network); !it.end(); ++it) {
        cModule *node = *it;
        
        // Cerca i moduli TDMASenderApp all'interno dei nodi
        for (cModule::SubmoduleIterator appIt(node); !appIt.end(); ++appIt) {
            cModule *app = *appIt;
            
            // Verifica se e' una senderApp configurata
            if (std::string(app->getName()) == "senderApp" && app->hasPar("flowId")) {
                std::string fid = app->par("flowId").stringValue();
                if (fid.empty()) continue; 

                Flow flow;
                flow.id = fid;
                flow.src = node->getName();
                flow.srcMac = app->par("srcAddr").stringValue();
                flow.dstMac = app->par("dstAddr").stringValue();
                
                flow.payload = app->par("payloadSize").intValue();
                flow.period = SimTime(app->par("period").doubleValue());
                flow.fragmentCount = app->par("burstSize").intValue();
                flow.isFragmented = flow.fragmentCount > 1;

                // Gestione Destinazione (Multicast vs Unicast)
                if (app->hasPar("destinations") && std::string(app->par("destinations").stringValue()) != "") {
                    flow.dst = app->par("destinations").stringValue();
                } else if (app->hasPar("dstNode")) {
                    flow.dst = app->par("dstNode").stringValue();
                } else {
                    EV_WARN << "Flow " << fid << " senza dstNode o destinations!" << endl;
                    flow.dst = "UNKNOWN";
                }

                flows.push_back(flow);
                EV << "Flow: " << flow.id << " [" << flow.src 
                   << " -> " << flow.dst << "] Period:" << flow.period << endl;
            }
        }
    }
    
    std::cout << "TDMA SCHEDULER: " << flows.size() << " flussi trovati" << std::endl;
}

std::vector<std::string> TDMAScheduler::getPathTo(const std::string& src, const std::string& dst) {
    // Controlla cache
    auto cacheKey = std::make_pair(src, dst);
    if (pathCache.find(cacheKey) != pathCache.end()) {
        return pathCache[cacheKey];
    }
    
    // BFS per trovare il path piu' breve
    std::queue<std::string> q;
    std::map<std::string, std::string> parent;
    
    q.push(src);
    parent[src] = "";
    
    while (!q.empty()) {
        std::string curr = q.front();
        q.pop();
        
        if (curr == dst) {
            // Ricostruisci path
            std::vector<std::string> path;
            for (std::string n = dst; !n.empty(); n = parent[n]) {
                path.push_back(n);
            }
            std::reverse(path.begin(), path.end());
            
            // Salva in cache
            pathCache[cacheKey] = path;
            return path;
        }
        
        // Esplora vicini
        if (adjacency.find(curr) != adjacency.end()) {
            for (const auto& neighborPair : adjacency[curr]) {
                const std::string& neighbor = neighborPair.first;
                if (parent.find(neighbor) == parent.end()) {
                    parent[neighbor] = curr;
                    q.push(neighbor);
                }
            }
        }
    }
    
    // Path non trovato
    EV_ERROR << "Path non trovato: " << src << " -> " << dst << endl;
    return {};
}

void TDMAScheduler::generateOptimizedSchedule() {
    std::vector<Job> jobs;

    for (auto& flow : flows) {
        simtime_t txTime = calculateTxTime(flow.payload);
        flow.txTime = txTime;

        int numTransmissions = std::max(1, (int)(hyperperiod / flow.period));
        if (hyperperiod < flow.period) numTransmissions = 1;

        // Parsing destinazioni multicast
        std::vector<std::string> destinations;
        if (flow.dst.find(',') != std::string::npos) {
            std::stringstream ss(flow.dst);
            std::string d;
            while(std::getline(ss, d, ',')) destinations.push_back(d);
        } else {
            destinations.push_back(flow.dst);
        }

        // Creazione Jobs
        for (int i = 0; i < numTransmissions; i++) {
            simtime_t release = i * flow.period;
            simtime_t deadline = (i + 1) * flow.period;
            
            for (int k = 0; k < flow.fragmentCount; k++) {
                Job job;
                job.flowId = flow.id;
                job.srcNode = flow.src;
                job.releaseTime = release;
                job.deadline = deadline;
                job.txDuration = txTime;
                job.fragmentIndex = k;
                job.destinations = destinations;
                jobs.push_back(job);
            }
        }
    }

    // Ordinamento EDF
    std::sort(jobs.begin(), jobs.end(), [](const Job& a, const Job& b) {
        if (a.deadline != b.deadline) return a.deadline < b.deadline;
        return a.releaseTime < b.releaseTime;
    });

    int totalJobs = jobs.size();
    EV << "Jobs totali da schedulare: " << totalJobs << endl;
    std::cout << "Jobs da schedulare: " << totalJobs << std::endl;
    int processed = 0;
    int lastPercent = -1;

    // Scheduling
    for (const auto& job : jobs) {
        processed++;
        int percent = (processed * 100) / totalJobs;
        if (percent % 10 == 0 && percent != lastPercent) {
            std::cout << "Scheduling... " << percent << "%" << std::endl;
            lastPercent = percent;
        }

        simtime_t t = job.releaseTime;
        bool scheduled = false;
        
        while (!scheduled) {
            if (t > hyperperiod * 1.5) {
                EV_ERROR << "Impossibile schedulare job " << job.flowId << endl;
                break;
            }

            bool pathFree = true;
            std::map<std::string, simtime_t> linkArrivals;

            // Calcolo tempi arrivo sui link per tutte le destinazioni
            for (const auto& dest : job.destinations) {
                std::vector<std::string> path = getPathTo(job.srcNode, dest);
                if (path.empty()) {
                    EV_ERROR << "Path non trovato: " << job.srcNode << " -> " << dest << endl;
                    continue; 
                }
                
                simtime_t hopTime = t;
                for (size_t i = 0; i < path.size() - 1; i++) {
                    std::string u = path[i];
                    std::string v = path[i+1];
                    std::string linkId = u + "->" + v;
                    
                    if (i > 0) hopTime += switchDelay;
                    
                    if (linkArrivals.find(linkId) == linkArrivals.end()) 
                        linkArrivals[linkId] = hopTime;
                    
                    hopTime += job.txDuration + propagationDelay;
                }
            }

            // Verifica collisioni
            for (const auto& entry : linkArrivals) {
                if (!isLinkFree(entry.first, entry.second, job.txDuration, guardTime)) {
                    pathFree = false;
                    break;
                }
            }

            if (pathFree) {
                scheduled = true;
                schedule.push_back({job.flowId, job.srcNode, t, job.txDuration, SLOT_SENDER});
                
                for (const auto& entry : linkArrivals) {
                    reserveLink(entry.first, entry.second, job.txDuration, guardTime);
                    std::string senderNode = entry.first.substr(0, entry.first.find("->"));
                    if (senderNode.find("switch") != std::string::npos) {
                        schedule.push_back({job.flowId, senderNode, entry.second, job.txDuration, SLOT_SWITCH});
                    }
                }
            } else {
                t += 1e-6; // Retry
            }
        }
    }
}

simtime_t TDMAScheduler::calculateTxTime(int payloadBytes) {
    int totalBytes = payloadBytes + tdma::ETHERNET_OVERHEAD;
    return SimTime((double)(totalBytes * 8) / datarate, SIMTIME_S);
}

void TDMAScheduler::configureSenders() {
    cModule *network = getParentModule();

    for (const auto& flow : flows) {
        cModule* node = network->getSubmodule(flow.src.c_str());
        if (!node) continue;

        for (cModule::SubmoduleIterator appIt(node); !appIt.end(); ++appIt) {
            cModule *app = *appIt;
            if (std::string(app->getName()) == "senderApp" && 
                std::string(app->par("flowId").stringValue()) == flow.id) {
                
                std::stringstream offsets;
                bool first = true;
                std::vector<simtime_t> flowSlots;
                
                for (const auto& slot : schedule) {
                    if (slot.flowId == flow.id && slot.node == flow.src && slot.type == SLOT_SENDER) {
                        flowSlots.push_back(slot.offset);
                    }
                }
                std::sort(flowSlots.begin(), flowSlots.end());
                
                for (const auto& t : flowSlots) {
                    if (!first) offsets << ",";
                    offsets << t.dbl();
                    first = false;
                }
                
                app->par("tdmaSlots").setStringValue(offsets.str());
                app->par("txDuration").setDoubleValue(flow.txTime.dbl());
                app->par("hyperperiod").setDoubleValue(hyperperiod.dbl());
            }
        }
    }
}

void TDMAScheduler::configureSwitches() {
    std::map<std::string, std::map<std::string, std::string>> switchTables;
    
    // Lambda per aggiungere entry
    auto addEntry = [&](const std::string& sw, const std::string& mac, int port) {
        std::string pStr = std::to_string(port);
        if (switchTables[sw][mac].empty()) {
            switchTables[sw][mac] = pStr;
        } else if (switchTables[sw][mac].find(pStr) == std::string::npos) {
            switchTables[sw][mac] += ";" + pStr;
        }
    };
    
    // Per ogni switch nella topologia
    for (const auto& adjEntry : adjacency) {
        const std::string& nodeName = adjEntry.first;
        
        // Solo switch
        if (nodeName.find("switch") == std::string::npos) continue;
        
        // Per ogni MAC address nella rete
        for (const auto& macEntry : nodeMacAddress) {
            const std::string& targetNode = macEntry.first;
            const std::string& targetMac = macEntry.second;
            
            // Salta se stesso (switch non hanno MAC nella nostra mappa)
            if (targetNode == nodeName) continue;
            
            // Trova path da questo switch al nodo target
            std::vector<std::string> path = getPathTo(nodeName, targetNode);
            if (path.size() < 2) continue;
            
            // Il primo hop dopo lo switch indica la direzione
            std::string nextHop = path[1];
            
            // Trova la porta locale verso nextHop
            for (const auto& neighborPair : adjacency[nodeName]) {
                if (neighborPair.first == nextHop) {
                    addEntry(nodeName, targetMac, neighborPair.second);
                    break;
                }
            }
        }
    }
    
    // Gestione multicast: raccogli tutte le destinazioni multicast dai flow
    // e aggiungi entry "multicast" con le porte necessarie
    std::set<std::string> multicastDestinations;
    for (const auto& flow : flows) {
        if (flow.dstMac == "multicast") {
            // Parse destinazioni
            std::stringstream ss(flow.dst);
            std::string d;
            while (std::getline(ss, d, ',')) {
                multicastDestinations.insert(d);
            }
        }
    }
    
    // Per ogni switch, aggiungi porte multicast
    if (!multicastDestinations.empty()) {
        for (const auto& adjEntry : adjacency) {
            const std::string& switchName = adjEntry.first;
            if (switchName.find("switch") == std::string::npos) continue;
            
            std::set<int> multicastPorts;
            
            for (const std::string& dest : multicastDestinations) {
                std::vector<std::string> path = getPathTo(switchName, dest);
                if (path.size() < 2) continue;
                
                std::string nextHop = path[1];
                
                for (const auto& neighborPair : adjacency[switchName]) {
                    if (neighborPair.first == nextHop) {
                        multicastPorts.insert(neighborPair.second);
                        break;
                    }
                }
            }
            
            // Aggiungi tutte le porte multicast
            for (int port : multicastPorts) {
                addEntry(switchName, "multicast", port);
            }
        }
    }
    
    // Applica configurazione agli switch
    for (const auto& tableEntry : switchTables) {
        const std::string& switchName = tableEntry.first;
        cModule* sw = getParentModule()->getSubmodule(switchName.c_str());
        if (!sw) continue;
        
        std::stringstream config;
        bool first = true;
        for (const auto& macPortEntry : tableEntry.second) {
            if (!first) config << ",";
            config << macPortEntry.first << "->" << macPortEntry.second;
            first = false;
        }
        
        sw->par("macTableConfig").setStringValue(config.str());
        EV << "Switch " << switchName << " MAC table: " << config.str() << endl;
    }
    
    std::cout << "TDMA SCHEDULER: MAC table configurate per " 
              << switchTables.size() << " switch" << std::endl;
}

void TDMAScheduler::handleMessage(cMessage *msg) {
    delete msg;
}
