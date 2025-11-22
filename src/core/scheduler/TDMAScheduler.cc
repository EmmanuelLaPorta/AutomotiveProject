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



void TDMAScheduler::generateOptimizedSchedule() {
    EV << "Generazione schedule ottimizzato..." << endl;
    
    // Ordina flussi per priorità
    std::sort(flows.begin(), flows.end(), 
        [](const Flow& a, const Flow& b) {
            return a.priority < b.priority;
        });
    
    // Calcola frame size per ogni flusso
    for (auto& flow : flows) {
        if (flow.isFragmented) {
            // Per flussi frammentati, calcola tempo per UN frammento
            int fragmentSize = (flow.payload <= tdma::MTU_BYTES) ? 
                              flow.payload : tdma::MTU_BYTES;
            flow.txTime = calculateTxTime(fragmentSize);
            
            EV << "Flow " << flow.id << " frammentato: " 
               << flow.fragmentCount << " frammenti da " 
               << fragmentSize << " byte, txTime=" << flow.txTime << endl;
        } else {
            flow.txTime = calculateTxTime(flow.payload);
        }
    }
    
    std::map<std::string, simtime_t> nextAvailableTime;
    
    for (auto& flow : flows) {
        int numTransmissions = (int)(hyperperiod / flow.period);
        
        // Controlla se è multicast
        bool isMulticast = (flow.dst.find(',') != std::string::npos);
        std::vector<std::string> destinations;
        
        if (isMulticast) {
            // Parsing destinazioni multiple
            std::stringstream ss(flow.dst);
            std::string dest;
            while (std::getline(ss, dest, ',')) {
                destinations.push_back(dest);
            }
            EV << "Flow multicast " << flow.id << " verso " << destinations.size() << " destinazioni" << endl;
        }
        
        EV_DEBUG << "Scheduling " << flow.id << ": " << numTransmissions 
                 << " trasmissioni ogni " << flow.period << endl;
        
        for (int i = 0; i < numTransmissions; i++) {
            simtime_t idealOffset = i * flow.period;
            simtime_t senderStart = std::max(idealOffset, nextAvailableTime[flow.src]);
            
            // ORDINE CORRETTO: più specifico prima
            if (isMulticast && flow.isFragmented) {
                // ===== MULTICAST FRAMMENTATO (Flow 6) =====
                EV_DEBUG << "Scheduling multicast fragmented flow " << flow.id
                         << " transmission " << i << " with " << flow.fragmentCount 
                         << " fragments" << endl;

                // Determina quale destinazione serve questo slot (alterna)
                int destIdx = i % destinations.size();
                std::string destForThisSlot = destinations[destIdx];
                
                simtime_t burstDuration = flow.txTime * flow.fragmentCount +
                                          tdma::getIfgTime() * (flow.fragmentCount - 1);

                // Slot sender per UNA SOLA destinazione
                schedule.push_back({
                    .flowId = flow.id,
                    .node = flow.src,
                    .offset = senderStart,
                    .duration = burstDuration,
                    .type = SLOT_SENDER
                });

                nextAvailableTime[flow.src] = senderStart + burstDuration + tdma::getGuardTime();

                // Path specifico per questa destinazione
                std::vector<std::string> specificPath = getPathTo(flow.src, destForThisSlot);

                // UN SOLO slot switch per l'intero burst (pipeline)
                simtime_t switchTime = senderStart + flow.txTime + tdma::getPropagationDelay();
                
                for (size_t j = 1; j < specificPath.size() - 1; j++) {
                    std::string node = specificPath[j];
                    
                    if (node.find("switch") != std::string::npos) {
                        switchTime += tdma::getSwitchDelay();
                        simtime_t switchStart = std::max(switchTime, nextAvailableTime[node]);
                        
                        schedule.push_back({
                            .flowId = flow.id + "_" + destForThisSlot,
                            .node = node,
                            .offset = switchStart,
                            .duration = burstDuration,
                            .type = SLOT_SWITCH
                        });
                        
                        nextAvailableTime[node] = switchStart + burstDuration + tdma::getGuardTime();
                        switchTime = switchStart + burstDuration;
                    }
                }
                
            } else if (isMulticast) {
                // ===== MULTICAST NORMALE (Flow 2 - Audio) =====
                schedule.push_back({
                    .flowId = flow.id,
                    .node = flow.src,
                    .offset = senderStart,
                    .duration = flow.txTime * destinations.size(),
                    .type = SLOT_SENDER
                });
                
                // Aggiorna tempo disponibile considerando tutto il burst
                simtime_t burstDuration = flow.txTime * destinations.size() + 
                                         tdma::getIfgTime() * (destinations.size() - 1);
                nextAvailableTime[flow.src] = senderStart + burstDuration + tdma::getGuardTime();
                
                // Schedula switch per OGNI destinazione
                for (const auto& dest : destinations) {
                    std::vector<std::string> specificPath = getPathTo(flow.src, dest);
                    
                    simtime_t currentTime = senderStart + flow.txTime;
                    
                    for (size_t j = 1; j < specificPath.size() - 1; j++) {
                        std::string node = specificPath[j];
                        
                        if (node.find("switch") != std::string::npos) {
                            currentTime += tdma::getPropagationDelay() + tdma::getSwitchDelay();
                            simtime_t switchStart = std::max(currentTime, nextAvailableTime[node]);
                            
                            schedule.push_back({
                                .flowId = flow.id + "_" + dest,
                                .node = node,
                                .offset = switchStart,
                                .duration = flow.txTime,
                                .type = SLOT_SWITCH
                            });
                            
                            nextAvailableTime[node] = switchStart + flow.txTime + tdma::getGuardTime();
                            currentTime = switchStart + flow.txTime;
                        }
                    }
                }
                
            } else if (flow.isFragmented) {
                // ===== UNICAST FRAMMENTATO (Flow 4, 5, 8) =====
                simtime_t burstDuration = flow.txTime * flow.fragmentCount +
                                          tdma::getIfgTime() * (flow.fragmentCount - 1);

                EV_DEBUG << "Scheduling fragmented flow " << flow.id 
                         << " transmission " << i << " with " << flow.fragmentCount 
                         << " fragments, burst duration=" << burstDuration << endl;

                // Slot sender per l'intero burst
                schedule.push_back({
                    .flowId = flow.id,
                    .node = flow.src,
                    .offset = senderStart,
                    .duration = burstDuration,
                    .type = SLOT_SENDER
                });

                nextAvailableTime[flow.src] = senderStart + burstDuration + tdma::getGuardTime();
                
                // Gli switch processano i frammenti in pipeline
                // UN SOLO slot switch per l'intero burst
                simtime_t switchTime = senderStart + flow.txTime + tdma::getPropagationDelay();
                
                for (size_t j = 1; j < flow.path.size() - 1; j++) {
                    std::string node = flow.path[j];
                    
                    if (node.find("switch") != std::string::npos) {
                        switchTime += tdma::getSwitchDelay();
                        simtime_t switchStart = std::max(switchTime, nextAvailableTime[node]);
                        
                        // UN SOLO slot per l'intero burst
                        schedule.push_back({
                            .flowId = flow.id,
                            .node = node,
                            .offset = switchStart,
                            .duration = burstDuration,
                            .type = SLOT_SWITCH
                        });
                        
                        nextAvailableTime[node] = switchStart + burstDuration + tdma::getGuardTime();
                        switchTime = switchStart + burstDuration;
                    }
                }
                
            } else {
                // ===== UNICAST NORMALE (Flow 1, 3, 7) =====
                schedule.push_back({
                    .flowId = flow.id,
                    .node = flow.src,
                    .offset = senderStart,
                    .duration = flow.txTime,
                    .type = SLOT_SENDER
                });
                
                nextAvailableTime[flow.src] = senderStart + flow.txTime + tdma::getGuardTime();
                
                simtime_t currentTime = senderStart + flow.txTime;
                
                for (size_t j = 1; j < flow.path.size() - 1; j++) {
                    std::string node = flow.path[j];
                    
                    if (node.find("switch") != std::string::npos) {
                        currentTime += tdma::getPropagationDelay() + tdma::getSwitchDelay();
                        simtime_t switchStart = std::max(currentTime, nextAvailableTime[node]);
                        
                        schedule.push_back({
                            .flowId = flow.id,
                            .node = node,
                            .offset = switchStart,
                            .duration = flow.txTime,
                            .type = SLOT_SWITCH
                        });
                        
                        nextAvailableTime[node] = switchStart + flow.txTime + tdma::getGuardTime();
                        currentTime = switchStart + flow.txTime;
                    }
                }
            }
            
            // MODIFICA CRITICA: Resetta nextAvailableTime per permettere scheduling periodico
            // Evita che le trasmissioni successive vengano bloccate dall'avanzamento del tempo
            if (i < numTransmissions - 1) {
                simtime_t nextIdealStart = (i + 1) * flow.period;
                // Resetta solo se non c'è conflitto imminente
                if (nextAvailableTime[flow.src] < nextIdealStart) {
                    nextAvailableTime[flow.src] = nextIdealStart;
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

    // Mappa: nodeName -> indice sender da usare
    std::map<std::string, int> senderIndices;

    for (const auto& flow : flows) {
        // Determina indice sender per questo nodo
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

        for (const auto& slot : schedule) {
            bool matchesFlow = (slot.flowId == flow.id) && (slot.node == flow.src);
            
            if (matchesFlow && slot.type == SLOT_SENDER) {
                if (!first) offsets << ",";
                offsets << slot.offset.dbl();
                first = false;
                slotCount++;
            }
        }

        if (first) {
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
            
            // Configura burst size e destinazione
            if (flow.dst.find(',') != std::string::npos) {
                // Multicast
                int numDest = std::count(flow.dst.begin(), flow.dst.end(), ',') + 1;

                if (flow.isFragmented) {
                    // Multicast frammentato: burst = fragmentCount (PER UNA destinazione)
                    // Lo scheduler alloca slot multipli per destinazioni multiple
                    sender->par("burstSize").setIntValue(flow.fragmentCount);
                } else {
                    // Multicast normale: burst = num destinazioni
                    sender->par("burstSize").setIntValue(numDest);
                }
                sender->par("dstAddr").setStringValue("multicast");

                // Salva numero destinazioni per multicast frammentato
                if (flow.isFragmented) {
                    sender->par("numDestinations").setIntValue(numDest);
                }
            } else {
                // Unicast
                sender->par("burstSize").setIntValue(
                    flow.isFragmented ? flow.fragmentCount : 1
                );
                sender->par("dstAddr").setStringValue(flow.dstMac);
            }
            
            EV << "Configurato " << path << " (" << flow.id << "): "
               << slotCount << " slot, burst="
               << sender->par("burstSize").intValue()
               << (flow.isFragmented ? " (frammentato)" : "") << endl;
            
        } catch (cRuntimeError& e) {
            error("Errore configurazione %s: %s", path.c_str(), e.what());
        }
    }
}

void TDMAScheduler::configureSwitches() {
    std::map<std::string, std::map<std::string, int>> switchTables;
    
    // Switch1 MAC table
    switchTables["switch1"]["00:00:00:00:00:05"] = 0; // S1
    switchTables["switch1"]["00:00:00:00:00:03"] = 1; // LD1
    switchTables["switch1"]["00:00:00:00:00:07"] = 2; // CU
    switchTables["switch1"]["00:00:00:00:00:0A"] = 2; // LD2
    switchTables["switch1"]["00:00:00:00:00:08"] = 2; // S2
    switchTables["switch1"]["00:00:00:00:00:0B"] = 3; // ME
    switchTables["switch1"]["00:00:00:00:00:0D"] = 3; // S3
    switchTables["switch1"]["00:00:00:00:00:11"] = 2; // S4
    switchTables["switch1"]["00:00:00:00:00:06"] = 4; // HU
    switchTables["switch1"]["00:00:00:00:00:02"] = 5; // US1
    switchTables["switch1"]["00:00:00:00:00:09"] = 2; // US2
    switchTables["switch1"]["00:00:00:00:00:10"] = 2; // US3
    switchTables["switch1"]["00:00:00:00:00:0C"] = 3; // US4
    switchTables["switch1"]["00:00:00:00:00:04"] = 6; // CM1
    switchTables["switch1"]["00:00:00:00:00:01"] = 7; // TLM
    switchTables["switch1"]["00:00:00:00:00:12"] = 2; // RS1
    switchTables["switch1"]["00:00:00:00:00:0E"] = 3; // RS2
    switchTables["switch1"]["00:00:00:00:00:0F"] = 2; // RC
    
    // Switch2 MAC table
    switchTables["switch2"]["00:00:00:00:00:08"] = 1; // S2
    switchTables["switch2"]["00:00:00:00:00:0A"] = 2; // LD2
    switchTables["switch2"]["00:00:00:00:00:07"] = 3; // CU
    switchTables["switch2"]["00:00:00:00:00:03"] = 0; // LD1
    switchTables["switch2"]["00:00:00:00:00:05"] = 0; // S1
    switchTables["switch2"]["00:00:00:00:00:0B"] = 0; // ME
    switchTables["switch2"]["00:00:00:00:00:0D"] = 0; // S3
    switchTables["switch2"]["00:00:00:00:00:11"] = 4; // S4
    switchTables["switch2"]["00:00:00:00:00:06"] = 0; // HU
    switchTables["switch2"]["00:00:00:00:00:02"] = 0; // US1
    switchTables["switch2"]["00:00:00:00:00:09"] = 5; // US2
    switchTables["switch2"]["00:00:00:00:00:10"] = 4; // US3
    switchTables["switch2"]["00:00:00:00:00:0C"] = 0; // US4
    switchTables["switch2"]["00:00:00:00:00:04"] = 0; // CM1
    switchTables["switch2"]["00:00:00:00:00:01"] = 0; // TLM
    switchTables["switch2"]["00:00:00:00:00:12"] = 4; // RS1
    switchTables["switch2"]["00:00:00:00:00:0E"] = 0; // RS2
    switchTables["switch2"]["00:00:00:00:00:0F"] = 4; // RC
    
    // Switch3 MAC table  
    switchTables["switch3"]["00:00:00:00:00:0B"] = 0; // ME
    switchTables["switch3"]["00:00:00:00:00:0D"] = 2; // S3
    switchTables["switch3"]["00:00:00:00:00:05"] = 1; // S1
    switchTables["switch3"]["00:00:00:00:00:08"] = 1; // S2
    switchTables["switch3"]["00:00:00:00:00:11"] = 3; // S4
    switchTables["switch3"]["00:00:00:00:00:03"] = 1; // LD1
    switchTables["switch3"]["00:00:00:00:00:0A"] = 1; // LD2
    switchTables["switch3"]["00:00:00:00:00:07"] = 1; // CU
    switchTables["switch3"]["00:00:00:00:00:02"] = 1; // US1
    switchTables["switch3"]["00:00:00:00:00:09"] = 1; // US2
    switchTables["switch3"]["00:00:00:00:00:10"] = 3; // US3
    switchTables["switch3"]["00:00:00:00:00:0C"] = 4; // US4
    switchTables["switch3"]["00:00:00:00:00:04"] = 1; // CM1
    switchTables["switch3"]["00:00:00:00:00:12"] = 3; // RS1
    switchTables["switch3"]["00:00:00:00:00:0E"] = 5; // RS2
    switchTables["switch3"]["00:00:00:00:00:01"] = 1; // TLM
    switchTables["switch3"]["00:00:00:00:00:0F"] = 3; // RC
    switchTables["switch3"]["00:00:00:00:00:06"] = 1; // HU
    
    // Switch4 MAC table
    switchTables["switch4"]["00:00:00:00:00:11"] = 2; // S4
    switchTables["switch4"]["00:00:00:00:00:0B"] = 1; // ME
    switchTables["switch4"]["00:00:00:00:00:0D"] = 1; // S3
    switchTables["switch4"]["00:00:00:00:00:05"] = 0; // S1
    switchTables["switch4"]["00:00:00:00:00:08"] = 0; // S2
    switchTables["switch4"]["00:00:00:00:00:03"] = 0; // LD1
    switchTables["switch4"]["00:00:00:00:00:0A"] = 0; // LD2
    switchTables["switch4"]["00:00:00:00:00:07"] = 0; // CU
    switchTables["switch4"]["00:00:00:00:00:02"] = 0; // US1
    switchTables["switch4"]["00:00:00:00:00:09"] = 0; // US2
    switchTables["switch4"]["00:00:00:00:00:10"] = 3; // US3
    switchTables["switch4"]["00:00:00:00:00:0C"] = 1; // US4
    switchTables["switch4"]["00:00:00:00:00:04"] = 0; // CM1
    switchTables["switch4"]["00:00:00:00:00:12"] = 5; // RS1
    switchTables["switch4"]["00:00:00:00:00:0E"] = 1; // RS2
    switchTables["switch4"]["00:00:00:00:00:01"] = 0; // TLM
    switchTables["switch4"]["00:00:00:00:00:0F"] = 4; // RC
    switchTables["switch4"]["00:00:00:00:00:06"] = 0; // HU
    
    // Applica configurazione
    for (const auto& [switchName, macTable] : switchTables) {
        cModule* sw = getModuleByPath(switchName.c_str());
        if (!sw) continue;
        
        std::stringstream config;
        bool first = true;
        
        for (const auto& [mac, port] : macTable) {
            if (!first) config << ",";
            config << mac << "->" << port;
            first = false;
        }
        
        sw->par("macTableConfig").setStringValue(config.str());
        EV << "Configurato " << switchName << " con MAC table" << endl;
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
