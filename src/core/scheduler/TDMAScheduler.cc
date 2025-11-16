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
    // Flow 1: LiDAR → CU (CRITICO ADAS)
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
}

void TDMAScheduler::generateOptimizedSchedule() {
    EV << "Generazione schedule ottimizzato..." << endl;
    
    // Ordina flussi per priorità
    std::sort(flows.begin(), flows.end(), 
        [](const Flow& a, const Flow& b) {
            return a.priority < b.priority;
        });
    
    // Per ogni flusso, calcola gli slot
    for (auto& flow : flows) {
        int numTransmissions = (int)(hyperperiod / flow.period);
        flow.txTime = calculateTxTime(flow.payload);
        
        for (int i = 0; i < numTransmissions; i++) {
            simtime_t offset = i * flow.period;
            
            // Slot per il sender
            schedule.push_back({
                .flowId = flow.id,
                .node = flow.src,
                .offset = offset,
                .duration = flow.txTime,
                .type = SLOT_SENDER
            });
            
            // Slot per ogni switch nel path
            simtime_t hopOffset = offset + flow.txTime;
            for (size_t j = 1; j < flow.path.size() - 1; j++) {
                if (flow.path[j].find("switch") != std::string::npos) {
                    hopOffset += tdma::getSwitchDelay();
                    
                    schedule.push_back({
                        .flowId = flow.id,
                        .node = flow.path[j],
                        .offset = hopOffset,
                        .duration = flow.txTime,
                        .type = SLOT_SWITCH
                    });
                    
                    hopOffset += flow.txTime;
                }
            }
        }
    }
    
    // Verifica collisioni
    if (!verifyNoCollisions()) {
        error("Schedule con collisioni!");
    }
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
    for (const auto& flow : flows) {
        cModule* sender = getModuleByPath((flow.src + ".senderApp[0]").c_str());
        if (!sender) continue;
        
        // Crea stringa con gli offset
        std::stringstream offsets;
        bool first = true;
        
        for (const auto& slot : schedule) {
            if (slot.flowId == flow.id && slot.type == SLOT_SENDER) {
                if (!first) offsets << ",";
                offsets << slot.offset.dbl();
                first = false;
            }
        }
        
        sender->par("tdmaSlots").setStringValue(offsets.str());
        sender->par("txDuration").setDoubleValue(flow.txTime.dbl());
        
        EV << "Configurato " << flow.src << " con slot TDMA" << endl;
    }
}

void TDMAScheduler::configureSwitches() {
    // Configura le tabelle MAC per ogni switch
    std::map<std::string, std::map<std::string, int>> switchTables;
    
    // Switch1 MAC table (aggiornata per la nuova topologia)
    switchTables["switch1"]["00:00:00:00:00:03"] = 0; // LD1 on port 0
    switchTables["switch1"]["00:00:00:00:00:07"] = 1; // CU via switch2 on port 1
    switchTables["switch1"]["00:00:00:00:00:0A"] = 1; // LD2 via switch2 on port 1
    
    // Switch2 MAC table (aggiornata per la nuova topologia)
    switchTables["switch2"]["00:00:00:00:00:0A"] = 1; // LD2 on port 1
    switchTables["switch2"]["00:00:00:00:00:07"] = 2; // CU on port 2
    switchTables["switch2"]["00:00:00:00:00:03"] = 0; // LD1 via switch1 on port 0
    
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

simtime_t TDMAScheduler::calculateTxTime(int payloadBytes) {
    int totalBytes = payloadBytes + tdma::ETHERNET_OVERHEAD;
    uint64_t totalBits = totalBytes * 8;
    return SimTime((double)totalBits / datarate, SIMTIME_S);
}

void TDMAScheduler::handleMessage(cMessage *msg) {
    error("TDMAScheduler non dovrebbe ricevere messaggi");
}