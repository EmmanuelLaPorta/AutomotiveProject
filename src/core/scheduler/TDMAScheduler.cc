// src/core/scheduler/TDMAScheduler.cc
#include "TDMAScheduler.h"
#include "../common/Constants.h"
#include <algorithm>
#include <sstream>

Define_Module(TDMAScheduler);

void TDMAScheduler::initialize() {
    hyperperiod = par("hyperperiod");
    datarate = par("datarate").doubleValue();
    
    EV << "=== TDMA Scheduler Initialization ===" << endl;
    EV << "Hyperperiod: " << hyperperiod << endl;
    
    // Definisci i flussi
    defineFlows();
    
    // Genera schedule offline
    generateOptimizedSchedule();
    
    // Configura i nodi
    configureSenders();
    configureSwitches();
    
    EV << "=== TDMA Schedule completato ===" << endl;
}

void TDMAScheduler::defineFlows() {
    // Flow 1a: LD1 → CU (ADAS)
    flows.push_back({
        .id = "flow1_LD1",
        .src = "LD1",
        .dst = "CU",
        .srcMac = "00:00:00:00:00:03",
        .dstMac = "00:00:00:00:00:07",
        .period = SimTime(1.4, SIMTIME_MS),
        .payload = 1300,
        .priority = tdma::PRIO_CRITICAL_SAFETY,
        .path = {"LD1", "switch1", "switch2", "CU"},
        .txTime = SimTime(0),
        .isFragmented = false,
        .fragmentCount = 1
    });
    
    //Flow 1b: LD2 → CU (ADAS)
    flows.push_back({
        .id = "flow1_LD2",
        .src = "LD2", 
        .dst = "CU",
        .srcMac = "00:00:00:00:00:0A",
        .dstMac = "00:00:00:00:00:07",
        .period = SimTime(1.4, SIMTIME_MS),
        .payload = 1300,
        .priority = tdma::PRIO_CRITICAL_SAFETY,
        .path = {"LD2", "switch2", "CU"},
        .txTime = SimTime(0),
        .isFragmented = false,
        .fragmentCount = 1
    });

    // Flow 2: ME → 4 Speaker (MULTICAST AUDIO)
    flows.push_back({
        .id = "flow2_multicast",
        .src = "ME",
        .dst = "S1,S2,S3,S4",
        .srcMac = "00:00:00:00:00:0B",
        .dstMac = "multicast",
        .period = SimTime(250, SIMTIME_US),
        .payload = 80,
        .priority = tdma::PRIO_INFOTAINMENT,
        .path = {"ME", "switch3"},
        .txTime = SimTime(0),
        .isFragmented = false,
        .fragmentCount = 1
    });
    

    // Flow 3a: US1 → CU (Parking sensor)
    flows.push_back({
        .id = "flow3_US1",
        .src = "US1",
        .dst = "CU",
        .srcMac = "00:00:00:00:00:02",
        .dstMac = "00:00:00:00:00:07",
        .period = SimTime(100, SIMTIME_MS),
        .payload = 188,
        .priority = tdma::PRIO_BEST_EFFORT,
        .path = {"US1", "switch1", "switch2", "CU"},
        .txTime = SimTime(0),
        .isFragmented = false,
        .fragmentCount = 1
    });

    // Flow 3b: US2 → CU
    flows.push_back({
        .id = "flow3_US2",
        .src = "US2",
        .dst = "CU",
        .srcMac = "00:00:00:00:00:09",
        .dstMac = "00:00:00:00:00:07",
        .period = SimTime(100, SIMTIME_MS),
        .payload = 188,
        .priority = tdma::PRIO_BEST_EFFORT,
        .path = {"US2", "switch2", "CU"},
        .txTime = SimTime(0),
        .isFragmented = false,
        .fragmentCount = 1
    });

    // Flow 3c: US3 → CU
    flows.push_back({
        .id = "flow3_US3",
        .src = "US3",
        .dst = "CU",
        .srcMac = "00:00:00:00:00:10",
        .dstMac = "00:00:00:00:00:07",
        .period = SimTime(100, SIMTIME_MS),
        .payload = 188,
        .priority = tdma::PRIO_BEST_EFFORT,
        .path = {"US3", "switch4", "switch2", "CU"},
        .txTime = SimTime(0),
        .isFragmented = false,
        .fragmentCount = 1
    });

    // Flow 3d: US4 → CU
    flows.push_back({
        .id = "flow3_US4",
        .src = "US4",
        .dst = "CU",
        .srcMac = "00:00:00:00:00:0C",
        .dstMac = "00:00:00:00:00:07",
        .period = SimTime(100, SIMTIME_MS),
        .payload = 188,
        .priority = tdma::PRIO_BEST_EFFORT,
        .path = {"US4", "switch3", "switch1", "switch2", "CU"},
        .txTime = SimTime(0),
        .isFragmented = false,
        .fragmentCount = 1
    });


    // Flow 4: CU → HU (CONTROL DATA - FRAGMENTED)
    flows.push_back({
        .id = "flow4",
        .src = "CU",
        .dst = "HU",
        .srcMac = "00:00:00:00:00:07",
        .dstMac = "00:00:00:00:00:06",
        .period = SimTime(10, SIMTIME_MS),
        .payload = 10500,
        .priority = tdma::PRIO_CONTROL,
        .path = {"CU", "switch2", "switch1", "HU"},
        .txTime = SimTime(0),
        .isFragmented = true,
        .fragmentCount = 7  // 10500 / 1500 = 7 frammenti
    });

    // Flow 5: CM1 → HU (Front Camera RAW 60 FPS - FRAGMENTED)
    flows.push_back({
        .id = "flow5",
        .src = "CM1",
        .dst = "HU",
        .srcMac = "00:00:00:00:00:04",
        .dstMac = "00:00:00:00:00:06",
        .period = SimTime(16.66, SIMTIME_MS),
        .payload = 178500,
        .priority = tdma::PRIO_CRITICAL_SAFETY,
        .path = {"CM1", "switch1", "HU"},
        .txTime = SimTime(0),
        .isFragmented = true,
        .fragmentCount = 119  // 178500 / 1500 = 119 frammenti
    });


    // Flow 6: ME -> RS1, RS2 (VIDEO STREAMING MULTICAST FRAMMENTED)
    flows.push_back({
        .id = "flow6_multicast",
        .src = "ME",
        .dst = "RS1,RS2",
        .srcMac = "00:00:00:00:00:0B",
        .dstMac = "multicast",
        .period = SimTime(33.33, SIMTIME_MS),
        .payload = 178500,
        .priority = tdma::PRIO_INFOTAINMENT,
        .path = {"ME", "switch3"},
        .txTime = SimTime(0),
        .isFragmented = true,
        .fragmentCount = 119
    });

    // Flow 7a: TLM -> HU (TELEMATICS)
    flows.push_back({
        .id = "flow7_HU",
        .src = "TLM",
        .dst = "HU",
        .srcMac = "00:00:00:00:00:01",
        .dstMac = "00:00:00:00:00:06",
        .period = SimTime(625, SIMTIME_US),
        .payload = 600,
        .priority = tdma::PRIO_CONTROL,
        .path = {"TLM", "switch1", "HU"},
        .txTime = SimTime(0),
        .isFragmented = false,
        .fragmentCount = 1
    });

    // Flow 7b: TLM -> CU (TELEMATICS)
    flows.push_back({
        .id = "flow7_CU",
        .src = "TLM",
        .dst = "CU",
        .srcMac = "00:00:00:00:00:01",
        .dstMac = "00:00:00:00:00:07",
        .period = SimTime(625, SIMTIME_US),
        .payload = 600,
        .priority = tdma::PRIO_CONTROL,
        .path = {"TLM", "switch1", "switch2", "CU"},
        .txTime = SimTime(0),
        .isFragmented = false,
        .fragmentCount = 1
    });

    // Flow 8: RC -> HU (REAR CAMERA - FRAGMENTED)
    flows.push_back({
        .id = "flow8",
        .src = "RC",
        .dst = "HU",
        .srcMac = "00:00:00:00:00:0F",
        .dstMac = "00:00:00:00:00:06",
        .period = SimTime(33.33, SIMTIME_MS),
        .payload = 178000,
        .priority = tdma::PRIO_CRITICAL_SAFETY,
        .path = {"RC", "switch4", "switch2", "switch1", "HU"},
        .txTime = SimTime(0),
        .isFragmented = true,
        .fragmentCount = 119
    });

}



// Struttura per un singolo job di trasmissione (frammento)
struct Job {
    std::string flowId;
    std::string srcNode;
    simtime_t releaseTime;
    simtime_t deadline;
    simtime_t txDuration;
    int priority;
    int fragmentIndex;
    int totalFragments;
    bool isMulticast;
    std::vector<std::string> destinations;
    std::vector<std::string> path; // Path principale (o primo path per multicast)
};

void TDMAScheduler::generateOptimizedSchedule() {
    EV << "Generazione schedule ottimizzato (INTERLEAVED)..." << endl;
    
    std::vector<Job> jobs;
    
    // 1. Generazione di tutti i Job (frammenti) nel periodo
    for (auto& flow : flows) {
        int numTransmissions = (int)(hyperperiod / flow.period);
        
        // Calcola txTime per frammento
        simtime_t fragmentTxTime;
        if (flow.isFragmented) {
             int fragmentSize = (flow.payload <= tdma::MTU_BYTES) ? flow.payload : tdma::MTU_BYTES;
             fragmentTxTime = calculateTxTime(fragmentSize);
        } else {
             fragmentTxTime = calculateTxTime(flow.payload);
        }
        flow.txTime = fragmentTxTime; // Aggiorna flow struct per uso futuro

        // Multicast parsing
        bool isMulticast = (flow.dst.find(',') != std::string::npos);
        std::vector<std::string> destinations;
        if (isMulticast) {
            std::stringstream ss(flow.dst);
            std::string dest;
            while (std::getline(ss, dest, ',')) destinations.push_back(dest);
        } else {
            destinations.push_back(flow.dst);
        }

        for (int i = 0; i < numTransmissions; i++) {
            simtime_t releaseTime = i * flow.period;
            simtime_t deadline = (i + 1) * flow.period;
            
            int numFragments = flow.isFragmented ? flow.fragmentCount : 1;
            
            for (int k = 0; k < numFragments; k++) {
                Job job;
                job.flowId = flow.id;
                job.srcNode = flow.src;
                job.releaseTime = releaseTime; // Tutti i frammenti pronti all'inizio del periodo
                job.deadline = deadline;
                job.txDuration = fragmentTxTime;
                job.priority = flow.priority;
                job.fragmentIndex = k;
                job.totalFragments = numFragments;
                job.isMulticast = isMulticast;
                job.destinations = destinations;
                job.path = flow.path;
                
                jobs.push_back(job);
            }
        }
    }
    
    // 2. Ordinamento Jobs
    // Priorità: Classe (basso valore = alta priorità) -> Release Time -> Deadline
    std::sort(jobs.begin(), jobs.end(), [](const Job& a, const Job& b) {
        if (a.priority != b.priority) return a.priority < b.priority;
        if (a.releaseTime != b.releaseTime) return a.releaseTime < b.releaseTime;
        return a.deadline < b.deadline;
    });
    
    EV << "Totale Jobs da schedulare: " << jobs.size() << endl;
    
    // 3. Scheduling Frame-by-Frame
    // Mappa per tracciare occupazione link: Node -> NextAvailableTime
    std::map<std::string, simtime_t> nodeFreeTime;
    
    for (const auto& job : jobs) {
        // Cerca primo slot valido per il sender
        simtime_t candidateStart = std::max(job.releaseTime, nodeFreeTime[job.srcNode]);
        
        bool scheduled = false;
        simtime_t bestStart = candidateStart;
        
        // Tentativo di trovare uno slot che funzioni per TUTTO il percorso (Path Delay Compensation)
        // Per semplicità, in questa implementazione statica, avanziamo finché non troviamo spazio su tutti i link.
        // Un approccio più sofisticato userebbe time-windows.
        
        while (!scheduled) {
            // Verifica disponibilità Sender
            if (bestStart < nodeFreeTime[job.srcNode]) {
                bestStart = nodeFreeTime[job.srcNode];
            }
            
            // Calcola tempi arrivo previsti agli switch
            simtime_t currentArrival = bestStart + job.txDuration + tdma::getPropagationDelay();
            bool pathFree = true;
            simtime_t requiredDelay = 0; // Ritardo aggiuntivo necessario se uno switch è occupato
            
            // Per multicast, dobbiamo verificare TUTTI i path verso le destinazioni
            // Qui semplifichiamo controllando i nodi coinvolti.
            // Recuperiamo tutti i nodi unici coinvolti nella trasmissione
            std::vector<std::string> nodesToCheck;
            
            // Costruiamo l'albero di trasmissione (semplificato: unione dei path)
            // Nota: job.path è solo UN path. Per multicast dobbiamo iterare le destinazioni.
            
            // Mappa: Node -> ArrivalTimeAtNode
            std::map<std::string, simtime_t> arrivalTimes;
            
            for (const auto& dest : job.destinations) {
                std::vector<std::string> path = getPathTo(job.srcNode, dest);
                simtime_t pathTime = currentArrival; // Tempo arrivo al primo switch
                
                for (size_t j = 1; j < path.size() - 1; j++) { // Salta src e dst
                    std::string sw = path[j];
                    if (sw.find("switch") != std::string::npos) {
                        if (arrivalTimes.find(sw) == arrivalTimes.end()) {
                            arrivalTimes[sw] = pathTime;
                        }
                        // Avanza tempo per prossimo hop
                        pathTime += tdma::getSwitchDelay() + tdma::getPropagationDelay(); 
                    }
                }
            }
            
            // Verifica disponibilità su tutti gli switch coinvolti
            for (const auto& entry : arrivalTimes) {
                std::string sw = entry.first;
                simtime_t arrival = entry.second;
                
                if (arrival < nodeFreeTime[sw]) {
                    // Switch occupato! Dobbiamo posticipare il sender.
                    // Quanto? Almeno (nodeFreeTime[sw] - arrival)
                    simtime_t delayNeeded = nodeFreeTime[sw] - arrival;
                    if (delayNeeded > requiredDelay) requiredDelay = delayNeeded;
                    pathFree = false;
                }
            }
            
            if (pathFree) {
                // Trovato slot valido! Registra schedule.
                scheduled = true;
                
                // 1. Sender Slot
                schedule.push_back({
                    .flowId = job.flowId,
                    .node = job.srcNode,
                    .offset = bestStart,
                    .duration = job.txDuration,
                    .type = SLOT_SENDER
                });
                nodeFreeTime[job.srcNode] = bestStart + job.txDuration + tdma::getGuardTime();
                
                // 2. Switch Slots
                for (const auto& entry : arrivalTimes) {
                    std::string sw = entry.first;
                    simtime_t arrival = entry.second;
                    
                    schedule.push_back({
                        .flowId = job.flowId,
                        .node = sw,
                        .offset = arrival,
                        .duration = job.txDuration,
                        .type = SLOT_SWITCH
                    });
                    nodeFreeTime[sw] = arrival + job.txDuration + tdma::getGuardTime();
                }
                
            } else {
                // Riprova più tardi
                bestStart += requiredDelay + tdma::getGuardTime(); // Aggiungi un piccolo step
                
                // Safety check loop infinito
                if (bestStart > job.deadline) {
                    EV_WARN << "Job " << job.flowId << " missed deadline!" << endl;
                    // Forziamo schedule o droppiamo? Per ora scheduliamo comunque per vedere l'errore
                    // scheduled = true; // Uncomment to force
                    // break;
                }
            }
        }
    }
    
    EV << "Schedule generato: " << schedule.size() << " slot totali" << endl;
    
    if (!verifyNoCollisions()) {
        printScheduleDebug();
        error("Schedule con collisioni!");
    }
}

simtime_t TDMAScheduler::findNextAvailableSlot(const std::string& node, simtime_t preferredTime, simtime_t duration) {
    simtime_t candidateTime = preferredTime;
    bool collision = true;
    int maxAttempts = 1000;
    int attempt = 0;
    
    while (collision && attempt < maxAttempts) {
        collision = false;
        
        for (const auto& slot : schedule) {
            if (slot.node != node) continue;
            
            simtime_t slotEnd = slot.offset + slot.duration + tdma::getGuardTime();
            simtime_t candidateEnd = candidateTime + duration;
            
            if ((candidateTime >= slot.offset && candidateTime < slotEnd) ||
                (candidateEnd > slot.offset && candidateEnd <= slotEnd) ||
                (candidateTime <= slot.offset && candidateEnd >= slotEnd)) {
                
                collision = true;
                candidateTime = slotEnd;
                break;
            }
        }
        
        attempt++;
    }
    
    if (attempt >= maxAttempts) {
        error("Impossibile trovare slot disponibile per nodo %s", node.c_str());
    }
    
    return candidateTime;
}

bool TDMAScheduler::verifyNoCollisions() {
    std::map<std::string, std::vector<Slot>> nodeSlots;
    
    for (const auto& slot : schedule) {
        nodeSlots[slot.node].push_back(slot);
    }
    
    for (const auto& [node, slots] : nodeSlots) {
        auto sortedSlots = slots;
        std::sort(sortedSlots.begin(), sortedSlots.end(),
            [](const Slot& a, const Slot& b) {
                return a.offset < b.offset;
            });
        
        for (size_t i = 0; i < sortedSlots.size() - 1; i++) {
            simtime_t endTime = sortedSlots[i].offset + sortedSlots[i].duration + tdma::getIfgTime();
            if (endTime > sortedSlots[i+1].offset) {
                EV_ERROR << "Collisione in " << node << " tra slot " << i << " e " << i+1 << endl;
                return false;
            }
        }
    }
    
    return true;
}

void TDMAScheduler::configureSenders() {
    EV << "=== Configurazione Senders ===" << endl;

    std::map<std::string, int> senderIndices;

    for (const auto& flow : flows) {
        int senderIdx = senderIndices[flow.src]++;
        std::string path = flow.src + ".senderApp[" + std::to_string(senderIdx) + "]";
        cModule* sender = getModuleByPath(path.c_str());
        
        if (!sender) {
            EV_WARN << "Modulo " << path << " non trovato!" << endl;
            continue;
        }

        // Raccogli slot per questo sender
        std::stringstream offsets;
        bool first = true;
        int slotCount = 0;

        // Ordina slot per tempo
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
            slotCount++;
        }

        if (slotCount == 0) {
            EV_WARN << "Nessuno slot trovato per " << flow.id << endl;
            continue;
        }
        
        try {
            sender->par("tdmaSlots").setStringValue(offsets.str());
            sender->par("txDuration").setDoubleValue(flow.txTime.dbl());
            sender->par("flowId").setStringValue(flow.id);
            sender->par("srcAddr").setStringValue(flow.srcMac);
            sender->par("payloadSize").setIntValue(
                flow.isFragmented ? tdma::MTU_BYTES : flow.payload
            );
            
            // Configura burst size = 1 (Interleaved)
            // Con scheduling frame-by-frame, ogni slot è un singolo pacchetto/frammento
            sender->par("burstSize").setIntValue(1);
            
            if (flow.dst.find(',') != std::string::npos) {
                sender->par("dstAddr").setStringValue("multicast");
                // Per multicast, il sender invia 1 pacchetto che lo switch duplica
                int numDest = std::count(flow.dst.begin(), flow.dst.end(), ',') + 1;
                sender->par("numDestinations").setIntValue(numDest);
            } else {
                sender->par("dstAddr").setStringValue(flow.dstMac);
            }
            
            EV << "Configurato " << path << " (" << flow.id << "): "
               << slotCount << " slot (interleaved)" << endl;
            
        } catch (cRuntimeError& e) {
            error("Errore configurazione %s: %s", path.c_str(), e.what());
        }
    }
}

void TDMAScheduler::configureSwitches() {
    std::map<std::string, std::map<std::string, std::string>> switchTables; // MAC -> PortsString
    
    // Helper lambda per aggiungere entry
    auto addEntry = [&](std::string sw, std::string mac, int port) {
        if (switchTables[sw][mac].empty()) {
            switchTables[sw][mac] = std::to_string(port);
        } else {
            // Se esiste già, appendi (multicast)
            // Controlla se porta già presente
            std::string current = switchTables[sw][mac];
            std::string pStr = std::to_string(port);
            if (current.find(pStr) == std::string::npos) {
                switchTables[sw][mac] += ";" + pStr;
            }
        }
    };

    // Configurazione Switch 1
    addEntry("switch1", "00:00:00:00:00:05", 0); // S1
    addEntry("switch1", "00:00:00:00:00:03", 1); // LD1
    addEntry("switch1", "00:00:00:00:00:07", 2); // CU
    addEntry("switch1", "00:00:00:00:00:0A", 2); // LD2 (via SW2)
    addEntry("switch1", "00:00:00:00:00:08", 2); // S2 (via SW2)
    addEntry("switch1", "00:00:00:00:00:0B", 3); // ME
    addEntry("switch1", "00:00:00:00:00:0D", 3); // S3 (via SW3)
    addEntry("switch1", "00:00:00:00:00:11", 2); // S4 (via SW2)
    addEntry("switch1", "00:00:00:00:00:06", 4); // HU
    addEntry("switch1", "00:00:00:00:00:02", 5); // US1
    addEntry("switch1", "00:00:00:00:00:09", 2); // US2 (via SW2)
    addEntry("switch1", "00:00:00:00:00:10", 2); // US3 (via SW2)
    addEntry("switch1", "00:00:00:00:00:0C", 3); // US4 (via SW3)
    addEntry("switch1", "00:00:00:00:00:04", 6); // CM1
    addEntry("switch1", "00:00:00:00:00:01", 7); // TLM
    addEntry("switch1", "00:00:00:00:00:12", 2); // RS1 (via SW2)
    addEntry("switch1", "00:00:00:00:00:0E", 3); // RS2 (via SW3)
    addEntry("switch1", "00:00:00:00:00:0F", 2); // RC (via SW2)
    
    // Switch 2
    addEntry("switch2", "00:00:00:00:00:08", 1); // S2
    addEntry("switch2", "00:00:00:00:00:0A", 2); // LD2
    addEntry("switch2", "00:00:00:00:00:07", 3); // CU
    addEntry("switch2", "00:00:00:00:00:03", 0); // LD1 (via SW1)
    addEntry("switch2", "00:00:00:00:00:05", 0); // S1 (via SW1)
    addEntry("switch2", "00:00:00:00:00:0B", 0); // ME (via SW1)
    addEntry("switch2", "00:00:00:00:00:0D", 0); // S3 (via SW1->SW3)
    addEntry("switch2", "00:00:00:00:00:11", 4); // S4 (via SW4)
    addEntry("switch2", "00:00:00:00:00:06", 0); // HU (via SW1)
    addEntry("switch2", "00:00:00:00:00:02", 0); // US1 (via SW1)
    addEntry("switch2", "00:00:00:00:00:09", 5); // US2
    addEntry("switch2", "00:00:00:00:00:10", 4); // US3 (via SW4)
    addEntry("switch2", "00:00:00:00:00:0C", 0); // US4 (via SW1->SW3)
    addEntry("switch2", "00:00:00:00:00:04", 0); // CM1 (via SW1)
    addEntry("switch2", "00:00:00:00:00:01", 0); // TLM (via SW1)
    addEntry("switch2", "00:00:00:00:00:12", 4); // RS1 (via SW4)
    addEntry("switch2", "00:00:00:00:00:0E", 0); // RS2 (via SW1->SW3)
    addEntry("switch2", "00:00:00:00:00:0F", 4); // RC (via SW4)

    // Switch 3
    addEntry("switch3", "00:00:00:00:00:0B", 0); // ME
    addEntry("switch3", "00:00:00:00:00:0D", 2); // S3
    addEntry("switch3", "00:00:00:00:00:05", 1); // S1 (via SW1)
    addEntry("switch3", "00:00:00:00:00:08", 1); // S2 (via SW1->SW2)
    addEntry("switch3", "00:00:00:00:00:11", 3); // S4 (via SW4)
    addEntry("switch3", "00:00:00:00:00:03", 1); // LD1 (via SW1)
    addEntry("switch3", "00:00:00:00:00:0A", 1); // LD2 (via SW1->SW2)
    addEntry("switch3", "00:00:00:00:00:07", 1); // CU (via SW1->SW2)
    addEntry("switch3", "00:00:00:00:00:02", 1); // US1 (via SW1)
    addEntry("switch3", "00:00:00:00:00:09", 1); // US2 (via SW1->SW2)
    addEntry("switch3", "00:00:00:00:00:10", 3); // US3 (via SW4)
    addEntry("switch3", "00:00:00:00:00:0C", 4); // US4
    addEntry("switch3", "00:00:00:00:00:04", 1); // CM1 (via SW1)
    addEntry("switch3", "00:00:00:00:00:12", 3); // RS1 (via SW4)
    addEntry("switch3", "00:00:00:00:00:0E", 5); // RS2
    addEntry("switch3", "00:00:00:00:00:01", 1); // TLM (via SW1)
    addEntry("switch3", "00:00:00:00:00:0F", 3); // RC (via SW4)
    addEntry("switch3", "00:00:00:00:00:06", 1); // HU (via SW1)

    // Switch 4
    addEntry("switch4", "00:00:00:00:00:11", 2); // S4
    addEntry("switch4", "00:00:00:00:00:0B", 1); // ME (via SW3)
    addEntry("switch4", "00:00:00:00:00:0D", 1); // S3 (via SW3)
    addEntry("switch4", "00:00:00:00:00:05", 0); // S1 (via SW2->SW1)
    addEntry("switch4", "00:00:00:00:00:08", 0); // S2 (via SW2)
    addEntry("switch4", "00:00:00:00:00:03", 0); // LD1 (via SW2->SW1)
    addEntry("switch4", "00:00:00:00:00:0A", 0); // LD2 (via SW2)
    addEntry("switch4", "00:00:00:00:00:07", 0); // CU (via SW2)
    addEntry("switch4", "00:00:00:00:00:02", 0); // US1 (via SW2->SW1)
    addEntry("switch4", "00:00:00:00:00:09", 0); // US2 (via SW2)
    addEntry("switch4", "00:00:00:00:00:10", 3); // US3
    addEntry("switch4", "00:00:00:00:00:0C", 1); // US4 (via SW3)
    addEntry("switch4", "00:00:00:00:00:04", 0); // CM1 (via SW2->SW1)
    addEntry("switch4", "00:00:00:00:00:12", 5); // RS1
    addEntry("switch4", "00:00:00:00:00:0E", 1); // RS2 (via SW3)
    addEntry("switch4", "00:00:00:00:00:01", 0); // TLM (via SW2->SW1)
    addEntry("switch4", "00:00:00:00:00:0F", 4); // RC
    addEntry("switch4", "00:00:00:00:00:06", 0); // HU (via SW2->SW1)

    // MULTICAST ENTRIES
    // Flow 2: ME (SW3) -> S1(SW1), S2(SW2), S3(SW3), S4(SW4)
    // SW3: In 0 (ME) -> Out 1(SW1/2), 2(S3), 3(SW4)
    addEntry("switch3", "multicast", 1);
    addEntry("switch3", "multicast", 2);
    addEntry("switch3", "multicast", 3);
    
    // SW1: In 3 (SW3) -> Out 0(S1), 2(SW2)
    addEntry("switch1", "multicast", 0);
    addEntry("switch1", "multicast", 2);
    
    // SW2: In 0 (SW1) -> Out 1(S2), 4(SW4)
    addEntry("switch2", "multicast", 1);
    addEntry("switch2", "multicast", 4);
    
    // SW4: In 1 (SW3) -> Out 2(S4)
    addEntry("switch4", "multicast", 2);
    
    // Flow 6: ME (SW3) -> RS1(SW4), RS2(SW3)
    // SW3: In 0 (ME) -> Out 3(SW4), 5(RS2)
    // Nota: "multicast" è già usato. Se i flow ID sono distinti, dovremmo usare flowId o MAC multicast distinti.
    // Qui assumiamo che "multicast" copra tutti. Questo è un limite della simulazione semplice.
    // Tuttavia, il TDMASwitch usa il MAC address.
    // Se entrambi i flow usano "multicast" come dstAddr, ci sarà flooding su tutte le porte configurate.
    // Soluzione ideale: usare MAC multicast diversi (es. 01:00:5E:00:00:01).
    // Per ora, aggiungiamo le porte anche per Flow 6.
    
    addEntry("switch3", "multicast", 5); // RS2
    addEntry("switch4", "multicast", 5); // RS1 (da SW3->SW4)

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
        EV << "Configurato " << switchName << " con MAC table (Multicast enabled)" << endl;
    }
}

void TDMAScheduler::printScheduleDebug() {
    std::map<std::string, std::vector<Slot>> nodeSlots;
    
    for (const auto& slot : schedule) {
        nodeSlots[slot.node].push_back(slot);
    }
    
    EV << "=== DEBUG SCHEDULE ===" << endl;
    for (const auto& [node, slots] : nodeSlots) {
        auto sorted = slots;
        std::sort(sorted.begin(), sorted.end(),
            [](const Slot& a, const Slot& b) { return a.offset < b.offset; });
        
        EV << node << " (" << sorted.size() << " slot):" << endl;
        for (size_t i = 0; i < std::min(sorted.size(), (size_t)5); i++) {
            EV << "  [" << i << "] t=" << sorted[i].offset 
               << " dur=" << sorted[i].duration 
               << " flow=" << sorted[i].flowId << endl;
        }
        if (sorted.size() > 5) EV << "  ... +" << (sorted.size()-5) << " slot" << endl;
    }
}

simtime_t TDMAScheduler::calculateTxTime(int payloadBytes) {
    int totalBytes = payloadBytes + tdma::ETHERNET_OVERHEAD;
    uint64_t totalBits = totalBytes * 8;
    return SimTime((double)totalBits / datarate, SIMTIME_S);
}

void TDMAScheduler::handleMessage(cMessage *msg) {
    error("TDMAScheduler non dovrebbe ricevere messaggi");
}

std::vector<std::string> TDMAScheduler::getPathTo(const std::string& src, const std::string& dst) {
    std::map<std::pair<std::string, std::string>, std::vector<std::string>> pathMap = {
        // Path per flow2 (ME -> S1, ... ,S4)
        {{"ME", "S1"}, {"ME", "switch3", "switch1", "S1"}},
        {{"ME", "S2"}, {"ME", "switch3", "switch1", "switch2", "S2"}},
        {{"ME", "S3"}, {"ME", "switch3", "S3"}},
        {{"ME", "S4"}, {"ME", "switch3", "switch4", "S4"}},
        // Path per flow6 (ME -> RS1, RS2)
        {{"ME", "RS1"}, {"ME", "switch3", "switch4", "RS1"}},
        {{"ME", "RS2"}, {"ME", "switch3", "RS2"}},
    };
    
    auto key = std::make_pair(src, dst);
    if (pathMap.find(key) != pathMap.end()) {
        return pathMap[key];
    }
    
    EV_WARN << "Path non trovato per " << src << " -> " << dst << endl;
    return {src, dst};
}
