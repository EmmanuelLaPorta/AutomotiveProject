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
    // Flow 1: LiDAR → CU (ADAS)
    flows.push_back({
        .id = "flow1_LD1",
        .src = "LD1",
        .dst = "CU",
        .srcMac = "00:00:00:00:00:03",
        .dstMac = "00:00:00:00:00:07",
        .period = SimTime(1.4, SIMTIME_MS),
        .payload = 1300,
        .priority = tdma::PRIO_CRITICAL_SAFETY,
        .path = {"LD1", "switch1", "switch2", "CU"}
    });
    
    flows.push_back({
        .id = "flow1_LD2",
        .src = "LD2", 
        .dst = "CU",
        .srcMac = "00:00:00:00:00:0A",
        .dstMac = "00:00:00:00:00:07",
        .period = SimTime(1.4, SIMTIME_MS),
        .payload = 1300,
        .priority = tdma::PRIO_CRITICAL_SAFETY,
        .path = {"LD2", "switch2", "CU"}
    });

    // Flow 2: ME → 4 Speaker (MULTICAST AUDIO)
    // Trattiamo come singolo flusso con 4 destinazioni
    flows.push_back({
        .id = "flow2_multicast",
        .src = "ME",
        .dst = "S1,S2,S3,S4",  // Indica destinazioni multiple
        .srcMac = "00:00:00:00:00:0B",
        .dstMac = "multicast",
        .period = SimTime(250, SIMTIME_US),
        .payload = 80,
        .priority = tdma::PRIO_INFOTAINMENT,
        .path = {"ME", "switch3"}  // Path comune iniziale
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
        flow.txTime = calculateTxTime(flow.payload);
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
            
            if (isMulticast) {
                // MULTICAST: alloca UN SOLO slot per ME con burst size = num destinazioni
                schedule.push_back({
                    .flowId = flow.id,
                    .node = flow.src,
                    .offset = senderStart,
                    .duration = flow.txTime * destinations.size(), // Tempo per tutte le trasmissioni
                    .type = SLOT_SENDER
                });
                
                // Aggiorna tempo disponibile considerando tutto il burst
                simtime_t burstDuration = flow.txTime * destinations.size() + 
                                         tdma::getGuardTime() * (destinations.size() - 1);
                nextAvailableTime[flow.src] = senderStart + burstDuration + tdma::getGuardTime();
                
                // Schedula switch per OGNI destinazione
                for (const auto& dest : destinations) {
                    // Determina path specifico
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
                
            } else {
                // UNICAST NORMALE (LiDAR)
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
        
        // Verifica se c'è collisione con slot esistenti
        for (const auto& slot : schedule) {
            if (slot.node != node) continue;
            
            simtime_t slotEnd = slot.offset + slot.duration + tdma::getGuardTime();
            simtime_t candidateEnd = candidateTime + duration;
            
            // Controlla sovrapposizione
            if ((candidateTime >= slot.offset && candidateTime < slotEnd) ||
                (candidateEnd > slot.offset && candidateEnd <= slotEnd) ||
                (candidateTime <= slot.offset && candidateEnd >= slotEnd)) {
                
                collision = true;
                // Sposta dopo questo slot
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
    // Raggruppa slot per nodo
    std::map<std::string, std::vector<Slot>> nodeSlots;
    
    for (const auto& slot : schedule) {
        nodeSlots[slot.node].push_back(slot);
    }
    
    // Verifica ogni nodo
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
    
    for (const auto& flow : flows) {
        std::string path = flow.src + ".senderApp[0]";
        cModule* sender = getModuleByPath(path.c_str());
        
        if (!sender) {
            EV_WARN << "Modulo " << path << " non trovato!" << endl;
            continue;
        }
        
        // Raccogli slot per questo sender
        std::stringstream offsets;
        bool first = true;
        
        for (const auto& slot : schedule) {
            if (slot.flowId == flow.id && slot.type == SLOT_SENDER) {
                if (!first) offsets << ",";
                offsets << slot.offset.dbl();
                first = false;
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
            sender->par("payloadSize").setIntValue(flow.payload);
            
            // Configura burst size e destinazione
            if (flow.dst.find(',') != std::string::npos) {
                // Multicast: conta destinazioni
                int numDest = std::count(flow.dst.begin(), flow.dst.end(), ',') + 1;
                sender->par("burstSize").setIntValue(numDest);
                sender->par("dstAddr").setStringValue("multicast");
            } else {
                // Unicast
                sender->par("burstSize").setIntValue(1);
                sender->par("dstAddr").setStringValue(flow.dstMac);
            }
            
            EV << "Configurato " << flow.src << " (" << flow.id << ")" << endl;
            
        } catch (cRuntimeError& e) {
            error("Errore configurazione %s: %s", path.c_str(), e.what());
        }
    }
}


void TDMAScheduler::configureSwitches() {
    std::map<std::string, std::map<std::string, int>> switchTables;
    
    // Switch1 MAC table
    switchTables["switch1"]["00:00:00:00:00:05"] = 0; // S1 on port 0
    switchTables["switch1"]["00:00:00:00:00:03"] = 1; // LD1 on port 1
    switchTables["switch1"]["00:00:00:00:00:07"] = 2; // CU via switch2
    switchTables["switch1"]["00:00:00:00:00:0A"] = 2; // LD2 via switch2
    switchTables["switch1"]["00:00:00:00:00:08"] = 2; // S2 via switch2
    switchTables["switch1"]["00:00:00:00:00:0B"] = 3; // ME via switch3
    switchTables["switch1"]["00:00:00:00:00:0D"] = 3; // S3 via switch3
    switchTables["switch1"]["00:00:00:00:00:11"] = 2; // S4 via switch2->switch4
    
    // Switch2 MAC table
    switchTables["switch2"]["00:00:00:00:00:08"] = 1; // S2 on port 1
    switchTables["switch2"]["00:00:00:00:00:0A"] = 2; // LD2 on port 2
    switchTables["switch2"]["00:00:00:00:00:07"] = 3; // CU on port 3
    switchTables["switch2"]["00:00:00:00:00:03"] = 0; // LD1 via switch1
    switchTables["switch2"]["00:00:00:00:00:05"] = 0; // S1 via switch1
    switchTables["switch2"]["00:00:00:00:00:0B"] = 0; // ME via switch1->switch3
    switchTables["switch2"]["00:00:00:00:00:0D"] = 0; // S3 via switch1->switch3
    switchTables["switch2"]["00:00:00:00:00:11"] = 4; // S4 via switch4
    
    // Switch3 MAC table  
    switchTables["switch3"]["00:00:00:00:00:0B"] = 0; // ME on port 0
    switchTables["switch3"]["00:00:00:00:00:0D"] = 2; // S3 on port 2
    switchTables["switch3"]["00:00:00:00:00:05"] = 1; // S1 via switch1
    switchTables["switch3"]["00:00:00:00:00:08"] = 1; // S2 via switch1->switch2
    switchTables["switch3"]["00:00:00:00:00:11"] = 3; // S4 via switch4
    switchTables["switch3"]["00:00:00:00:00:03"] = 1; // LD1 via switch1
    switchTables["switch3"]["00:00:00:00:00:0A"] = 1; // LD2 via switch1->switch2
    switchTables["switch3"]["00:00:00:00:00:07"] = 1; // CU via switch1->switch2
    
    // Switch4 MAC table
    switchTables["switch4"]["00:00:00:00:00:11"] = 2; // S4 on port 2
    switchTables["switch4"]["00:00:00:00:00:0B"] = 1; // ME via switch3
    switchTables["switch4"]["00:00:00:00:00:0D"] = 1; // S3 via switch3
    switchTables["switch4"]["00:00:00:00:00:05"] = 0; // S1 via switch2->switch1
    switchTables["switch4"]["00:00:00:00:00:08"] = 0; // S2 via switch2
    switchTables["switch4"]["00:00:00:00:00:03"] = 0; // LD1 via switch2->switch1
    switchTables["switch4"]["00:00:00:00:00:0A"] = 0; // LD2 via switch2
    switchTables["switch4"]["00:00:00:00:00:07"] = 0; // CU via switch2
    
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
    // Mappa dei path hard-coded per questa topologia
    std::map<std::pair<std::string, std::string>, std::vector<std::string>> pathMap = {
        // Da ME agli speaker
        {{"ME", "S1"}, {"ME", "switch3", "switch1", "S1"}},
        {{"ME", "S2"}, {"ME", "switch3", "switch1", "switch2", "S2"}},
        {{"ME", "S3"}, {"ME", "switch3", "S3"}},
        {{"ME", "S4"}, {"ME", "switch3", "switch4", "S4"}},
    };
    
    auto key = std::make_pair(src, dst);
    if (pathMap.find(key) != pathMap.end()) {
        return pathMap[key];
    }
    
    EV_WARN << "Path non trovato per " << src << " -> " << dst << endl;
    return {src, dst};
}
