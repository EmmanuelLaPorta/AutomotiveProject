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

    // Flow 2: ME → 4 Speaker (Audio INFOTAINMENT)
    // Ogni speaker è una destinazione separata per schedulazione
    flows.push_back({
        .id = "flow2_S1",
        .src = "ME",
        .dst = "S1", 
        .srcMac = "00:00:00:00:00:0B",
        .dstMac = "00:00:00:00:00:05",
        .period = SimTime(250, SIMTIME_US),  // 250 μs
        .payload = 80,  // Audio packet
        .priority = tdma::PRIO_INFOTAINMENT,
        .path = {"ME", "switch3", "switch1", "S1"}
    });

    flows.push_back({
        .id = "flow2_S2",
        .src = "ME",
        .dst = "S2",
        .srcMac = "00:00:00:00:00:0B", 
        .dstMac = "00:00:00:00:00:08",
        .period = SimTime(250, SIMTIME_US),
        .payload = 80,
        .priority = tdma::PRIO_INFOTAINMENT,
        .path = {"ME", "switch3", "switch1", "switch2", "S2"}
    });

    flows.push_back({
        .id = "flow2_S3",
        .src = "ME",
        .dst = "S3",
        .srcMac = "00:00:00:00:00:0B",
        .dstMac = "00:00:00:00:00:0D",
        .period = SimTime(250, SIMTIME_US),
        .payload = 80,
        .priority = tdma::PRIO_INFOTAINMENT,
        .path = {"ME", "switch3", "S3"}  // Diretto
    });

    flows.push_back({
        .id = "flow2_S4",
        .src = "ME",
        .dst = "S4",
        .srcMac = "00:00:00:00:00:0B",
        .dstMac = "00:00:00:00:00:11",
        .period = SimTime(250, SIMTIME_US),
        .payload = 80,
        .priority = tdma::PRIO_INFOTAINMENT,
        .path = {"ME", "switch3", "switch4", "S4"}
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
    // Configura tutti i sender basandosi sui flussi definiti
    std::map<std::string, std::vector<simtime_t>> senderSlots;
    std::map<std::string, simtime_t> senderTxTime;
    
    // Raccogli slot per ogni sender
    for (const auto& flow : flows) {
        int numTransmissions = (int)(hyperperiod / flow.period);
        for (int i = 0; i < numTransmissions; i++) {
            simtime_t offset = i * flow.period;
            senderSlots[flow.src].push_back(offset);
        }
        senderTxTime[flow.src] = flow.txTime;
    }
    
    // Configura ME per multicast (4 destinazioni)
    cModule* me = getModuleByPath("ME.senderApp[0]");
    if (me) {
        // ME trasmette a 4 destinazioni contemporaneamente
        std::stringstream offsets;
        bool first = true;
        
        // Usa gli slot del primo flusso audio (sono tutti uguali)
        int numTransmissions = (int)(hyperperiod / SimTime(250, SIMTIME_US));
        for (int i = 0; i < numTransmissions; i++) {
            if (!first) offsets << ",";
            offsets << (i * 0.00025);  // 250 μs in secondi
            first = false;
        }
        
        me->par("tdmaSlots").setStringValue(offsets.str());
        me->par("txDuration").setDoubleValue(calculateTxTime(80).dbl());
        me->par("burstSize").setIntValue(4);  // 4 frame per slot (uno per speaker)
        me->par("flowId").setStringValue("flow2");
        me->par("srcAddr").setStringValue("00:00:00:00:00:0B");
        me->par("dstAddr").setStringValue("multicast");  // Destinazione multicast
        me->par("payloadSize").setIntValue(80);
        
        EV << "Configurato ME per multicast audio" << endl;
    }
    
    // Configura LiDAR come prima
    for (const auto& flow : flows) {
        if (flow.src == "LD1" || flow.src == "LD2") {
            cModule* sender = getModuleByPath((flow.src + ".senderApp[0]").c_str());
            if (!sender) continue;
            
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




simtime_t TDMAScheduler::calculateTxTime(int payloadBytes) {
    int totalBytes = payloadBytes + tdma::ETHERNET_OVERHEAD;
    uint64_t totalBits = totalBytes * 8;
    return SimTime((double)totalBits / datarate, SIMTIME_S);
}

void TDMAScheduler::handleMessage(cMessage *msg) {
    error("TDMAScheduler non dovrebbe ricevere messaggi");
}