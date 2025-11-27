// Implementazione scheduler
#include "TDMAScheduler.h"
#include "../common/Constants.h" 
#include <algorithm>
#include <sstream>
#include <set>
#include <map>
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
    int priority;
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

    // Reset strutture
    flows.clear();
    schedule.clear();
    linkTable.clear();

    try { tdma::setGuardTime(guardTime); } catch (...) {}

    std::cout << "TDMA SCHEDULER: Acquisizione dei flussi in corso..." << std::endl;

    // 1. Leggo configurazione dall'ambiente (INI/NED)
    discoverFlowsFromNetwork();
    
    // 2. Calcolo tabella
    generateOptimizedSchedule();
    
    // 3. Distribuisco configurazione
    configureSenders();
    configureSwitches();
    
    std::cout << "TDMA SCHEDULER: Inizializzazione completata" << std::endl;
}

void TDMAScheduler::discoverFlowsFromNetwork() {
    cModule *network = getParentModule(); 
    
    // Itera su tutti i nodi della rete
    for (cModule::SubmoduleIterator it(network); !it.end(); ++it) {
        cModule *node = *it;
        
        // Cerca i moduli TDMASenderApp all'interno dei nodi
        for (cModule::SubmoduleIterator appIt(node); !appIt.end(); ++appIt) {
            cModule *app = *appIt;
            
            // Verifica se è una senderApp configurata
            if (std::string(app->getName()) == "senderApp" && app->hasPar("flowId")) {
                std::string fid = app->par("flowId").stringValue();
                if (fid.empty()) continue; 

                Flow flow;
                flow.id = fid;
                flow.src = node->getName(); // Es. "LD1"
                flow.srcMac = app->par("srcAddr").stringValue();
                flow.dstMac = app->par("dstAddr").stringValue();
                
                // Leggi parametri aggiunti nel NED/INI
                flow.payload = app->par("payloadSize").intValue();
                flow.period = SimTime(app->par("period").doubleValue());
                flow.priority = app->par("priority").intValue();
                flow.fragmentCount = app->par("burstSize").intValue();
                flow.isFragmented = flow.fragmentCount > 1;

                // Gestione Destinazione (Multicast vs Unicast) per il routing
                if (app->hasPar("destinations") && std::string(app->par("destinations").stringValue()) != "") {
                    // Multicast: legge lista nodi (es. "S1,S2,S3")
                    flow.dst = app->par("destinations").stringValue();
                } else if (app->hasPar("dstNode")) {
                    // Unicast: legge nome nodo (es. "CU")
                    flow.dst = app->par("dstNode").stringValue();
                } else {
                    // Fallback 
                    EV_WARN << "Flow " << fid << " senza dstNode o destinations configurato!" << endl;
                    flow.dst = "UNKNOWN";
                }

                flows.push_back(flow);
                EV << "Flow Trovato: " << flow.id << " [Src:" << flow.src 
                   << " -> Dst:" << flow.dst << "] Period:" << flow.period << endl;
            }
        }
    }
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
                job.priority = flow.priority;
                job.fragmentIndex = k;
                job.destinations = destinations;
                jobs.push_back(job);
            }
        }
    }

    // Ordinamento EDF
    std::sort(jobs.begin(), jobs.end(), [](const Job& a, const Job& b) {
        if (a.deadline != b.deadline) return a.deadline < b.deadline;
        if (a.priority != b.priority) return a.priority < b.priority;
        return a.releaseTime < b.releaseTime;
    });

    // Variabili per progress bar
        int totalJobs = jobs.size();
        EV << "Jobs totali da schedulare: " << totalJobs << endl;
        std::cout << "Jobs da schedulare: " << totalJobs << std::endl;
        int processed = 0;
        int lastPercent = -1;

    // Scheduling
    for (const auto& job : jobs) {

        // Output progresso su console
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
                    
                    if (i > 0) hopTime += tdma::getSwitchDelay();
                    
                    if (linkArrivals.find(linkId) == linkArrivals.end()) 
                        linkArrivals[linkId] = hopTime;
                    
                    hopTime += job.txDuration + tdma::getPropagationDelay();
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

    // Per ogni flusso trovato, configura l'app corrispondente
    for (const auto& flow : flows) {
        // Cerca il modulo sorgente e la senderApp specifica
        cModule* node = network->getSubmodule(flow.src.c_str());
        if (!node) continue;

        for (cModule::SubmoduleIterator appIt(node); !appIt.end(); ++appIt) {
            cModule *app = *appIt;
            if (std::string(app->getName()) == "senderApp" && 
                std::string(app->par("flowId").stringValue()) == flow.id) {
                
                // Raccogli slot
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
                
                // Scrivi risultati
                app->par("tdmaSlots").setStringValue(offsets.str());
                app->par("txDuration").setDoubleValue(flow.txTime.dbl());
                app->par("hyperperiod").setDoubleValue(hyperperiod.dbl());
                
                
            }
        }
    }
}

void TDMAScheduler::configureSwitches() {
    
    std::map<std::string, std::map<std::string, std::string>> switchTables;
    
    // per popolare la tabella
    auto addEntry = [&](std::string sw, std::string mac, int port) {
        std::string pStr = std::to_string(port);
        if (switchTables[sw][mac].empty()) switchTables[sw][mac] = pStr;
        else if (switchTables[sw][mac].find(pStr) == std::string::npos) 
            switchTables[sw][mac] += ";" + pStr;
    };

    // NOTA: Qui mantengo la tua configurazione statica delle porte perché 
    // il routing automatico sugli switch richiederebbe di conoscere la topologia delle porte.
    // Se vuoi renderlo dinamico, servirebbe una funzione "discoverTopology".
    // Per ora, copio la tua configurazione manuale che è corretta per lo scenario fisso.
    
    // Switch 1
    addEntry("switch1", "00:00:00:00:00:05", 0); // S1
    addEntry("switch1", "00:00:00:00:00:03", 1); // LD1
    addEntry("switch1", "00:00:00:00:00:07", 2); // CU
    addEntry("switch1", "00:00:00:00:00:0A", 2); // LD2
    addEntry("switch1", "00:00:00:00:00:08", 2); // S2
    addEntry("switch1", "00:00:00:00:00:0B", 3); // ME
    addEntry("switch1", "00:00:00:00:00:0D", 3); // S3
    addEntry("switch1", "00:00:00:00:00:11", 2); // S4
    addEntry("switch1", "00:00:00:00:00:06", 4); // HU
    addEntry("switch1", "00:00:00:00:00:02", 5); // US1
    addEntry("switch1", "00:00:00:00:00:09", 2); // US2
    addEntry("switch1", "00:00:00:00:00:10", 2); // US3
    addEntry("switch1", "00:00:00:00:00:0C", 3); // US4
    addEntry("switch1", "00:00:00:00:00:04", 6); // CM1
    addEntry("switch1", "00:00:00:00:00:01", 7); // TLM
    addEntry("switch1", "00:00:00:00:00:12", 2); // RS1
    addEntry("switch1", "00:00:00:00:00:0E", 3); // RS2
    addEntry("switch1", "00:00:00:00:00:0F", 2); // RC
    addEntry("switch1", "multicast", 0);
    addEntry("switch1", "multicast", 2);

    // Switch 2
    addEntry("switch2", "00:00:00:00:00:08", 1); 
    addEntry("switch2", "00:00:00:00:00:0A", 2); 
    addEntry("switch2", "00:00:00:00:00:07", 3); 
    addEntry("switch2", "00:00:00:00:00:03", 0); 
    addEntry("switch2", "00:00:00:00:00:05", 0); 
    addEntry("switch2", "00:00:00:00:00:0B", 0); 
    addEntry("switch2", "00:00:00:00:00:0D", 0); 
    addEntry("switch2", "00:00:00:00:00:11", 4); 
    addEntry("switch2", "00:00:00:00:00:06", 0); 
    addEntry("switch2", "00:00:00:00:00:02", 0); 
    addEntry("switch2", "00:00:00:00:00:09", 5); 
    addEntry("switch2", "00:00:00:00:00:10", 4); 
    addEntry("switch2", "00:00:00:00:00:0C", 0); 
    addEntry("switch2", "00:00:00:00:00:04", 0); 
    addEntry("switch2", "00:00:00:00:00:01", 0); 
    addEntry("switch2", "00:00:00:00:00:12", 4); 
    addEntry("switch2", "00:00:00:00:00:0E", 0); 
    addEntry("switch2", "00:00:00:00:00:0F", 4); 
    addEntry("switch2", "multicast", 1);
    addEntry("switch2", "multicast", 4);

    // Switch 3
    addEntry("switch3", "00:00:00:00:00:0B", 0); 
    addEntry("switch3", "00:00:00:00:00:0D", 2); 
    addEntry("switch3", "00:00:00:00:00:05", 1); 
    addEntry("switch3", "00:00:00:00:00:08", 1); 
    addEntry("switch3", "00:00:00:00:00:11", 3); 
    addEntry("switch3", "00:00:00:00:00:03", 1); 
    addEntry("switch3", "00:00:00:00:00:0A", 1); 
    addEntry("switch3", "00:00:00:00:00:07", 1); 
    addEntry("switch3", "00:00:00:00:00:02", 1); 
    addEntry("switch3", "00:00:00:00:00:09", 1); 
    addEntry("switch3", "00:00:00:00:00:10", 3); 
    addEntry("switch3", "00:00:00:00:00:0C", 4); 
    addEntry("switch3", "00:00:00:00:00:04", 1); 
    addEntry("switch3", "00:00:00:00:00:12", 3); 
    addEntry("switch3", "00:00:00:00:00:0E", 5); 
    addEntry("switch3", "00:00:00:00:00:01", 1); 
    addEntry("switch3", "00:00:00:00:00:0F", 3); 
    addEntry("switch3", "00:00:00:00:00:06", 1); 
    addEntry("switch3", "multicast", 1);
    addEntry("switch3", "multicast", 2);
    addEntry("switch3", "multicast", 3);
    addEntry("switch3", "multicast", 5);

    // Switch 4
    addEntry("switch4", "00:00:00:00:00:11", 2); 
    addEntry("switch4", "00:00:00:00:00:0B", 1); 
    addEntry("switch4", "00:00:00:00:00:0D", 1); 
    addEntry("switch4", "00:00:00:00:00:05", 0); 
    addEntry("switch4", "00:00:00:00:00:08", 0); 
    addEntry("switch4", "00:00:00:00:00:03", 0); 
    addEntry("switch4", "00:00:00:00:00:0A", 0); 
    addEntry("switch4", "00:00:00:00:00:07", 0); 
    addEntry("switch4", "00:00:00:00:00:02", 0); 
    addEntry("switch4", "00:00:00:00:00:09", 0); 
    addEntry("switch4", "00:00:00:00:00:10", 3); 
    addEntry("switch4", "00:00:00:00:00:0C", 1); 
    addEntry("switch4", "00:00:00:00:00:04", 0); 
    addEntry("switch4", "00:00:00:00:00:12", 5); 
    addEntry("switch4", "00:00:00:00:00:0E", 1); 
    addEntry("switch4", "00:00:00:00:00:01", 0); 
    addEntry("switch4", "00:00:00:00:00:0F", 4); 
    addEntry("switch4", "00:00:00:00:00:06", 0); 
    addEntry("switch4", "multicast", 2);
    addEntry("switch4", "multicast", 5);

    // Applica configurazione
    for (const auto& [switchName, macTable] : switchTables) {
        cModule* sw = getModuleByPath(switchName.c_str());
        if (!sw) continue;
        std::stringstream config;
        bool first = true;
        for (const auto& [mac, ports] : macTable) {
            if (!first) config << ",";
            config << mac << "->" << ports;
            first = false;
        }
        sw->par("macTableConfig").setStringValue(config.str());
    }
}

std::vector<std::string> TDMAScheduler::getPathTo(const std::string& src, const std::string& dst) {
    // Mappa per il routing logico (Nome Nodo -> Nome Nodo)
    std::map<std::pair<std::string, std::string>, std::vector<std::string>> pathMap = {
        {{"LD1", "CU"}, {"LD1", "switch1", "switch2", "CU"}},
        {{"LD2", "CU"}, {"LD2", "switch2", "CU"}},
        {{"US1", "CU"}, {"US1", "switch1", "switch2", "CU"}},
        {{"US2", "CU"}, {"US2", "switch2", "CU"}},
        {{"US3", "CU"}, {"US3", "switch4", "switch2", "CU"}},
        {{"US4", "CU"}, {"US4", "switch3", "switch1", "switch2", "CU"}},
        {{"CU", "HU"}, {"CU", "switch2", "switch1", "HU"}},
        {{"CM1", "HU"}, {"CM1", "switch1", "HU"}},
        {{"RC", "HU"}, {"RC", "switch4", "switch2", "switch1", "HU"}},
        {{"TLM", "HU"}, {"TLM", "switch1", "HU"}},
        {{"TLM", "CU"}, {"TLM", "switch1", "switch2", "CU"}},
        // Multicast destinations
        {{"ME", "S1"}, {"ME", "switch3", "switch1", "S1"}},
        {{"ME", "S2"}, {"ME", "switch3", "switch1", "switch2", "S2"}},
        {{"ME", "S3"}, {"ME", "switch3", "S3"}},
        {{"ME", "S4"}, {"ME", "switch3", "switch4", "S4"}},
        {{"ME", "RS1"}, {"ME", "switch3", "switch4", "RS1"}},
        {{"ME", "RS2"}, {"ME", "switch3", "RS2"}},
    };
    
    if (pathMap.find({src, dst}) != pathMap.end()) {
        return pathMap[{src, dst}];
    }
    return {}; // Path non trovato
}

void TDMAScheduler::handleMessage(cMessage *msg) {
    delete msg; //Non serve gestire message qui, TDMA in questa fase è offline, lascio perché è obbligatoria per CSimpleModule
}
