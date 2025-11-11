#include "TDMAScheduler.h"
#include <cmath>
#include <algorithm>
#include <set>
#include <queue>

Define_Module(TDMAScheduler);

void TDMAScheduler::initialize() {
    hyperperiod = par("hyperperiod");
    datarate = par("datarate").doubleValue();
    overhead = par("ethernetOverhead").intValue();
    switchingDelay = par("switchingDelay");
    propagationDelay = par("propagationDelay");

    EV << "=== TDMA Path-Aware Scheduler Initialization ===" << endl;
    EV << "Hyperperiod: " << hyperperiod << endl;
    EV << "Datarate: " << datarate << " bps" << endl;
    EV << "Switching Delay: " << switchingDelay << endl;
    EV << "Propagation Delay: " << propagationDelay << endl;

    buildNetworkTopology();
    defineFlows();
    calculateFlowPaths();
    calculateTransmissionTimes();
    generateSchedule();

    if (!checkCollisions()) {
        error("TDMA Schedule ha collisioni! Impossibile continuare.");
    }

    assignOffsetsToSenders();

    EV << "=== TDMA Path-Aware Schedule completato con successo ===" << endl;
    EV << "Totale slot schedulati: " << schedule.size() << endl;
}

void TDMAScheduler::handleMessage(cMessage *msg) {
    error("TDMAScheduler non dovrebbe ricevere messaggi");
}

void TDMAScheduler::buildNetworkTopology() {
    EV << "=== Costruzione topologia di rete ===" << endl;
    
    // Mappa MAC → Nome nodo
    topology.macToNode["00:00:00:00:00:01"] = "TLM";
    topology.macToNode["00:00:00:00:00:02"] = "US1";
    topology.macToNode["00:00:00:00:00:03"] = "LD1";
    topology.macToNode["00:00:00:00:00:04"] = "CM1";
    topology.macToNode["00:00:00:00:00:05"] = "S1";
    topology.macToNode["00:00:00:00:00:06"] = "HU";
    topology.macToNode["00:00:00:00:00:07"] = "CU";
    topology.macToNode["00:00:00:00:00:08"] = "S2";
    topology.macToNode["00:00:00:00:00:09"] = "US2";
    topology.macToNode["00:00:00:00:00:0A"] = "LD2";
    topology.macToNode["00:00:00:00:00:0B"] = "ME";
    topology.macToNode["00:00:00:00:00:0C"] = "US4";
    topology.macToNode["00:00:00:00:00:0D"] = "S3";
    topology.macToNode["00:00:00:00:00:0E"] = "RS2";
    topology.macToNode["00:00:00:00:00:0F"] = "RC";
    topology.macToNode["00:00:00:00:00:10"] = "US3";
    topology.macToNode["00:00:00:00:00:11"] = "S4";
    topology.macToNode["00:00:00:00:00:12"] = "RS1";
    
    // ✅ CORREZIONE: Definizione connessioni BIDIREZIONALI esplicite
    
    // Switch1 - End Systems
    topology.connections["TLM"].push_back("switch1");
    topology.connections["switch1"].push_back("TLM");
    
    topology.connections["US1"].push_back("switch1");
    topology.connections["switch1"].push_back("US1");
    
    topology.connections["LD1"].push_back("switch1");
    topology.connections["switch1"].push_back("LD1");
    
    topology.connections["CM1"].push_back("switch1");
    topology.connections["switch1"].push_back("CM1");
    
    topology.connections["S1"].push_back("switch1");
    topology.connections["switch1"].push_back("S1");
    
    topology.connections["HU"].push_back("switch1");
    topology.connections["switch1"].push_back("HU");
    
    // Switch2 - End Systems
    topology.connections["CU"].push_back("switch2");
    topology.connections["switch2"].push_back("CU");
    
    topology.connections["S2"].push_back("switch2");
    topology.connections["switch2"].push_back("S2");
    
    topology.connections["US2"].push_back("switch2");
    topology.connections["switch2"].push_back("US2");
    
    topology.connections["LD2"].push_back("switch2");
    topology.connections["switch2"].push_back("LD2");
    
    // Switch3 - End Systems
    topology.connections["ME"].push_back("switch3");
    topology.connections["switch3"].push_back("ME");
    
    topology.connections["US4"].push_back("switch3");
    topology.connections["switch3"].push_back("US4");
    
    topology.connections["S3"].push_back("switch3");
    topology.connections["switch3"].push_back("S3");
    
    topology.connections["RS2"].push_back("switch3");
    topology.connections["switch3"].push_back("RS2");
    
    // Switch4 - End Systems
    topology.connections["RC"].push_back("switch4");
    topology.connections["switch4"].push_back("RC");
    
    topology.connections["US3"].push_back("switch4");
    topology.connections["switch4"].push_back("US3");
    
    topology.connections["S4"].push_back("switch4");
    topology.connections["switch4"].push_back("S4");
    
    topology.connections["RS1"].push_back("switch4");
    topology.connections["switch4"].push_back("RS1");
    
    // ✅ Inter-switch connections (BIDIREZIONALI)
    topology.connections["switch1"].push_back("switch2");
    topology.connections["switch2"].push_back("switch1");
    
    topology.connections["switch1"].push_back("switch3");
    topology.connections["switch3"].push_back("switch1");
    
    topology.connections["switch2"].push_back("switch4");
    topology.connections["switch4"].push_back("switch2");
    
    topology.connections["switch3"].push_back("switch4");
    topology.connections["switch4"].push_back("switch3");
    
    EV << "Topologia costruita: " << topology.macToNode.size() << " end-systems, 4 switch" << endl;
}

std::vector<TDMAScheduler::PathHop> TDMAScheduler::findPath(
    const std::string& srcMac, const std::string& dstMac) {
    
    std::vector<PathHop> path;
    
    std::string srcNode = topology.macToNode[srcMac];
    std::string dstNode = topology.macToNode[dstMac];
    
    EV_DEBUG << "Calcolo percorso: " << srcNode << " (" << srcMac << ") → " 
             << dstNode << " (" << dstMac << ")" << endl;
    
    // BFS per trovare il percorso più breve
    std::map<std::string, std::string> parent;
    std::queue<std::string> queue;
    std::set<std::string> visited;
    
    queue.push(srcNode);
    visited.insert(srcNode);
    parent[srcNode] = "";
    
    bool found = false;
    
    while (!queue.empty() && !found) {
        std::string current = queue.front();
        queue.pop();
        
        if (current == dstNode) {
            found = true;
            break;
        }
        
        // Esplora tutti i vicini
        auto it = topology.connections.find(current);
        if (it != topology.connections.end()) {
            for (const auto& neighbor : it->second) {
                if (visited.find(neighbor) == visited.end()) {
                    visited.insert(neighbor);
                    parent[neighbor] = current;
                    queue.push(neighbor);
                    
                    EV_DEBUG << "  BFS: " << current << " → " << neighbor << endl;
                }
            }
        }
    }
    
    // Ricostruisci il percorso
    if (!found || parent.find(dstNode) == parent.end()) {
        EV_ERROR << "Nessun percorso trovato tra " << srcNode << " e " << dstNode << endl;
        EV_ERROR << "Nodi visitati: " << visited.size() << endl;
        return path;
    }
    
    std::vector<std::string> reversePath;
    std::string current = dstNode;
    while (current != "") {
        reversePath.push_back(current);
        current = parent[current];
    }
    
    std::reverse(reversePath.begin(), reversePath.end());
    
    EV << "Percorso trovato (" << reversePath.size() << " nodi): ";
    for (const auto& node : reversePath) {
        EV << node << " → ";
    }
    EV << "END" << endl;
    
    // Costruisci PathHop
    for (size_t i = 0; i < reversePath.size(); ++i) {
        PathHop hop;
        hop.nodeName = reversePath[i];
        hop.ingressPort = -1;  // Semplificato
        hop.egressPort = -1;   // Semplificato
        
        // Calcola delay per questo hop
        if (reversePath[i].find("switch") != std::string::npos) {
            hop.hopDelay = switchingDelay + propagationDelay;
        } else {
            hop.hopDelay = propagationDelay;
        }
        
        path.push_back(hop);
    }
    
    return path;
}

void TDMAScheduler::defineFlows() {
    EV << "Definizione flussi..." << endl;

    // Flow 2: ME → S1-S4 (Audio speaker) - PRIORITÀ 1
    flows.push_back({
        "flow2_ME_to_S1", "ME.senderApp[0]", "00:00:00:00:00:0B", "00:00:00:00:00:05",
        SimTime(0.00025, SIMTIME_S), 80, 1, 1, 0, {}
    });
    flows.push_back({
        "flow2_ME_to_S2", "ME.senderApp[1]", "00:00:00:00:00:0B", "00:00:00:00:00:08",
        SimTime(0.00025, SIMTIME_S), 80, 1, 1, 0, {}
    });
    flows.push_back({
        "flow2_ME_to_S3", "ME.senderApp[2]", "00:00:00:00:00:0B", "00:00:00:00:00:0D",
        SimTime(0.00025, SIMTIME_S), 80, 1, 1, 0, {}
    });
    flows.push_back({
        "flow2_ME_to_S4", "ME.senderApp[3]", "00:00:00:00:00:0B", "00:00:00:00:00:11",
        SimTime(0.00025, SIMTIME_S), 80, 1, 1, 0, {}
    });

    // Flow 7: TLM → HU, CU - PRIORITÀ 2
    flows.push_back({
        "flow7_TLM_to_HU", "TLM.senderApp[0]", "00:00:00:00:00:01", "00:00:00:00:00:06",
        SimTime(0.000625, SIMTIME_S), 600, 1, 2, 0, {}
    });
    flows.push_back({
        "flow7_TLM_to_CU", "TLM.senderApp[1]", "00:00:00:00:00:01", "00:00:00:00:00:07",
        SimTime(0.000625, SIMTIME_S), 600, 1, 2, 0, {}
    });

    // Flow 1: LD1, LD2 → CU - PRIORITÀ 3
    flows.push_back({
        "flow1_LD1_to_CU", "LD1.senderApp[0]", "00:00:00:00:00:03", "00:00:00:00:00:07",
        SimTime(0.0014, SIMTIME_S), 1300, 1, 3, 0, {}
    });
    flows.push_back({
        "flow1b_LD2_to_CU", "LD2.senderApp[0]", "00:00:00:00:00:0A", "00:00:00:00:00:07",
        SimTime(0.0014, SIMTIME_S), 1300, 1, 3, 0, {}
    });

    // Flow 4: CU → HU - PRIORITÀ 4
    flows.push_back({
        "flow4_CU_to_HU", "CU.senderApp[0]", "00:00:00:00:00:07", "00:00:00:00:00:06",
        SimTime(0.01, SIMTIME_S), 1500, 7, 4, 0, {}
    });

    // Flow 5: CM1 → HU - PRIORITÀ 5
    flows.push_back({
        "flow5_CM1_to_HU", "CM1.senderApp[0]", "00:00:00:00:00:04", "00:00:00:00:00:06",
        SimTime(0.01666, SIMTIME_S), 1500, 119, 5, 0, {}
    });

    // Flow 6: ME → RS1, RS2 - PRIORITÀ 6
    flows.push_back({
        "flow6_ME_to_RS1", "ME.senderApp[4]", "00:00:00:00:00:0B", "00:00:00:00:00:12",
        SimTime(0.03333, SIMTIME_S), 1500, 119, 6, 0, {}
    });
    flows.push_back({
        "flow6_ME_to_RS2", "ME.senderApp[5]", "00:00:00:00:00:0B", "00:00:00:00:00:0E",
        SimTime(0.03333, SIMTIME_S), 1500, 119, 6, 0, {}
    });

    // Flow 8: RC → HU - PRIORITÀ 7
    flows.push_back({
        "flow8_RC_to_HU", "RC.senderApp[0]", "00:00:00:00:00:0F", "00:00:00:00:00:06",
        SimTime(0.03333, SIMTIME_S), 1500, 119, 7, 0, {}
    });

    // Flow 3: US1-4 → CU - PRIORITÀ 8
    flows.push_back({
        "flow3_US1_to_CU", "US1.senderApp[0]", "00:00:00:00:00:02", "00:00:00:00:00:07",
        SimTime(0.1, SIMTIME_S), 188, 1, 8, 0, {}
    });
    flows.push_back({
        "flow3_US2_to_CU", "US2.senderApp[0]", "00:00:00:00:00:09", "00:00:00:00:00:07",
        SimTime(0.1, SIMTIME_S), 188, 1, 8, 0, {}
    });
    flows.push_back({
        "flow3_US3_to_CU", "US3.senderApp[0]", "00:00:00:00:00:10", "00:00:00:00:00:07",
        SimTime(0.1, SIMTIME_S), 188, 1, 8, 0, {}
    });
    flows.push_back({
        "flow3_US4_to_CU", "US4.senderApp[0]", "00:00:00:00:00:0C", "00:00:00:00:00:07",
        SimTime(0.1, SIMTIME_S), 188, 1, 8, 0, {}
    });

    EV << "Totale flussi definiti: " << flows.size() << endl;
}

void TDMAScheduler::calculateFlowPaths() {
    EV << "=== Calcolo percorsi dei flussi ===" << endl;
    
    for (auto& flow : flows) {
        flow.path = findPath(flow.srcAddress, flow.dstAddress);
        
        if (flow.path.empty()) {
            error("Impossibile trovare percorso per %s", flow.flowName.c_str());
        }
        
        EV << "Flow " << flow.flowName << ": " << flow.path.size() << " hop" << endl;
    }
}

void TDMAScheduler::calculateTransmissionTimes() {
    EV << "Calcolo tempi di trasmissione..." << endl;

    for (auto &flow : flows) {
        flow.txTime = calculateTxTime(flow.payloadSize);
        EV << "Flow " << flow.flowName << ": T_tx = " << flow.txTime << " s" << endl;
    }
}

simtime_t TDMAScheduler::calculateTxTime(int payloadSize) {
    uint64_t totalBytes = payloadSize + overhead;
    uint64_t totalBits = totalBytes * 8;
    double txTimeSec = (double)totalBits / (double)datarate;
    return SimTime(txTimeSec, SIMTIME_S);
}

int TDMAScheduler::calculateTransmissionsInHyperperiod(simtime_t period) {
    return (int)floor(hyperperiod.dbl() / period.dbl());
}

simtime_t TDMAScheduler::calculateEndToEndDelay(const FlowDescriptor& flow) {
    simtime_t totalDelay = 0;
    
    // Trasmissione iniziale
    totalDelay += flow.txTime;
    
    // Ritardi degli hop intermedi (switch)
    for (const auto& hop : flow.path) {
        if (hop.nodeName.find("switch") != std::string::npos) {
            totalDelay += hop.hopDelay + flow.txTime; // switching + ritrasmissione
        }
    }
    
    return totalDelay;
}

int TDMAScheduler::determineEgressPort(const std::string& switchName, 
                                       const std::vector<PathHop>& path, 
                                       size_t currentHopIndex) {
    // Determina la porta di uscita in base al prossimo hop
    if (currentHopIndex + 1 >= path.size()) {
        return -1; // Ultimo hop
    }
    
    const std::string& nextHop = path[currentHopIndex + 1].nodeName;
    
    // Mappa delle connessioni inter-switch e switch-to-end-system
    // Basata sulla topologia nel .ned file
    
    if (switchName == "switch1") {
        if (nextHop == "TLM") return 0;
        if (nextHop == "US1") return 1;
        if (nextHop == "LD1") return 2;
        if (nextHop == "CM1") return 3;
        if (nextHop == "S1") return 4;
        if (nextHop == "HU") return 5;
        if (nextHop == "switch2") return 6;
        if (nextHop == "switch3") return 7;
    }
    else if (switchName == "switch2") {
        if (nextHop == "CU") return 0;
        if (nextHop == "S2") return 1;
        if (nextHop == "US2") return 2;
        if (nextHop == "LD2") return 3;
        if (nextHop == "switch1") return 4;
        if (nextHop == "switch4") return 5;
    }
    else if (switchName == "switch3") {
        if (nextHop == "ME") return 0;
        if (nextHop == "US4") return 1;
        if (nextHop == "S3") return 2;
        if (nextHop == "RS2") return 3;
        if (nextHop == "switch1") return 4;
        if (nextHop == "switch4") return 5;
    }
    else if (switchName == "switch4") {
        if (nextHop == "RC") return 0;
        if (nextHop == "US3") return 1;
        if (nextHop == "S4") return 2;
        if (nextHop == "switch2") return 3;
        if (nextHop == "switch3") return 4;
        if (nextHop == "RS1") return 5;
    }
    
    EV_WARN << "Porta non trovata per " << switchName << " → " << nextHop << endl;
    return -1;
}

void TDMAScheduler::generateSchedule()
{
    EV << "=== Generazione Schedule TDMA Path-Aware ===" << endl;

    struct Job {
        std::string flowName;
        std::string senderModule;
        std::string srcAddress;
        std::string dstAddress;
        int burstInstanceNumber;
        int fragmentNumber;
        simtime_t releaseTime;
        simtime_t deadline;
        simtime_t txTime;
        int priority;
        int totalFragments;
        std::vector<PathHop> path;
    };

    std::vector<Job> allJobs;

    // Genera job per ogni frammento di ogni burst
    for (const auto& flow : flows) {
        int numBursts = calculateTransmissionsInHyperperiod(flow.period);

        for (int burstNum = 0; burstNum < numBursts; ++burstNum) {
            simtime_t burstRelease = burstNum * flow.period;
            simtime_t burstDeadline = burstRelease + flow.period;

            for (int fragNum = 1; fragNum <= flow.burstSize; ++fragNum) {
                allJobs.push_back({
                    flow.flowName,
                    flow.senderModule,
                    flow.srcAddress,
                    flow.dstAddress,
                    burstNum,
                    fragNum,
                    burstRelease,
                    burstDeadline,
                    flow.txTime,
                    flow.priority,
                    flow.burstSize,
                    flow.path
                });
            }
        }
    }

    EV << "Totale job da schedulare: " << allJobs.size() << endl;

    // Scheduling con coordinamento end-to-end
    simtime_t currentTime = 0;
    const simtime_t IFG = SimTime(96, SIMTIME_NS);
    const simtime_t GUARD_TIME = SimTime(1000, SIMTIME_NS);
    
    std::set<int> scheduledJobs;
    int totalJobs = allJobs.size();
    int scheduledCount = 0;
    
    // Traccia occupazione di ogni porta dello switch
    std::map<std::string, simtime_t> portBusyUntil; // "switchX:portY" → tempo

    while (scheduledCount < totalJobs) {
        std::vector<int> readyJobIndices;
        
        for (size_t i = 0; i < allJobs.size(); ++i) {
            if (scheduledJobs.find(i) == scheduledJobs.end() &&
                allJobs[i].releaseTime <= currentTime) {
                readyJobIndices.push_back(i);
            }
        }

        if (readyJobIndices.empty()) {
            simtime_t nextRelease = hyperperiod;
            
            for (size_t i = 0; i < allJobs.size(); ++i) {
                if (scheduledJobs.find(i) == scheduledJobs.end() &&
                    allJobs[i].releaseTime > currentTime &&
                    allJobs[i].releaseTime < nextRelease) {
                    nextRelease = allJobs[i].releaseTime;
                }
            }
            
            if (nextRelease >= hyperperiod) break;
            currentTime = nextRelease;
            continue;
        }

        // Ordina per priorità
        std::sort(readyJobIndices.begin(), readyJobIndices.end(),
            [&allJobs](int a, int b) {
                const Job& jobA = allJobs[a];
                const Job& jobB = allJobs[b];
                
                if (jobA.priority != jobB.priority) {
                    return jobA.priority < jobB.priority;
                }
                
                if (jobA.flowName == jobB.flowName &&
                    jobA.burstInstanceNumber == jobB.burstInstanceNumber) {
                    return jobA.fragmentNumber < jobB.fragmentNumber;
                }
                
                if (jobA.deadline != jobB.deadline) {
                    return jobA.deadline < jobB.deadline;
                }
                
                return jobA.fragmentNumber < jobB.fragmentNumber;
            });

        // Schedula il job con priorità più alta
        int selectedIdx = readyJobIndices[0];
        const Job& selected = allJobs[selectedIdx];

        // CRITICAL: Coordina trasmissione attraverso tutti gli hop
        simtime_t hopStartTime = currentTime;
        
        // Slot per end-system (primo hop)
        TransmissionSlot slot;
        slot.flowName = selected.flowName;
        slot.senderModule = selected.senderModule;
        slot.offset = hopStartTime;
        slot.fragmentNumber = selected.fragmentNumber;
        slot.burstSize = selected.totalFragments;
        slot.instanceNumber = selected.burstInstanceNumber;
        slot.switchNode = "";
        slot.switchPort = -1;
        slot.isEndSystemTx = true;
        
        schedule.push_back(slot);
        
        // Calcola quando finisce la trasmissione end-system
        hopStartTime += selected.txTime + IFG;
        
        // ✅ MODIFICATO: Crea slot per OGNI switch nel percorso
        for (size_t hopIdx = 1; hopIdx < selected.path.size() - 1; ++hopIdx) {
            const PathHop& hop = selected.path[hopIdx];
            
            if (hop.nodeName.find("switch") != std::string::npos) {
                // Aggiungi delay di switching
                hopStartTime += hop.hopDelay;
                
                // Determina la porta di uscita dello switch
                // Per semplicità, usiamo una mappa predefinita delle porte
                int egressPort = determineEgressPort(hop.nodeName, selected.path, hopIdx);
                
                if (egressPort >= 0) {
                    // Crea slot per questo switch
                    TransmissionSlot switchSlot;
                    switchSlot.flowName = selected.flowName;
                    switchSlot.senderModule = "";
                    switchSlot.offset = hopStartTime;
                    switchSlot.fragmentNumber = selected.fragmentNumber;
                    switchSlot.burstSize = selected.totalFragments;
                    switchSlot.instanceNumber = selected.burstInstanceNumber;
                    switchSlot.switchNode = hop.nodeName;
                    switchSlot.switchPort = egressPort;
                    switchSlot.isEndSystemTx = false;
                    
                    schedule.push_back(switchSlot);
                    
                    EV_DEBUG << "Switch slot: " << hop.nodeName << ":" << egressPort 
                             << " a t=" << hopStartTime << " per " << selected.flowName << endl;
                }
                
                // Il frame viene ritrasmesso dallo switch
                hopStartTime += selected.txTime + IFG;
            }
        }

        scheduledJobs.insert(selectedIdx);
        scheduledCount++;

        // Avanza tempo
        currentTime = hopStartTime + GUARD_TIME;

        if (scheduledCount % 200 == 0) {
            EV << "Progress: " << scheduledCount << "/" << totalJobs 
               << " (" << (scheduledCount*100/totalJobs) << "%)" << endl;
        }
    }

    EV << "=== Schedule completato ===" << endl;
    EV << "Slot totali: " << schedule.size() << endl;
    EV << "Tempo finale: " << currentTime << endl;
    
    double utilizzo = (currentTime.dbl() / hyperperiod.dbl()) * 100;
    EV << "Utilizzo canale: " << utilizzo << "%" << endl;
}

bool TDMAScheduler::checkCollisions() {
    EV << "Verifica collisioni per end-system..." << endl;

    // Filtra solo slot di end-system
    std::vector<TransmissionSlot> endSystemSlots;
    for (const auto& slot : schedule) {
        if (slot.isEndSystemTx) {
            endSystemSlots.push_back(slot);
        }
    }

    std::sort(endSystemSlots.begin(), endSystemSlots.end(),
        [](const TransmissionSlot &a, const TransmissionSlot &b) {
            return a.offset < b.offset;
        });

    int collisionCount = 0;

    for (size_t i = 0; i < endSystemSlots.size() - 1; i++) {
        TransmissionSlot &current = endSystemSlots[i];
        TransmissionSlot &next = endSystemSlots[i + 1];

        auto it = std::find_if(flows.begin(), flows.end(),
            [&current](const FlowDescriptor &f) {
                return f.senderModule == current.senderModule;
            });

        if (it == flows.end()) continue;

        simtime_t endTime = current.offset + it->txTime + SimTime(96, SIMTIME_NS);

        if (endTime > next.offset) {
            collisionCount++;
            EV_ERROR << "COLLISIONE: " << current.flowName 
                     << " → " << next.flowName << endl;
        }
    }

    if (collisionCount == 0) {
        EV << "Nessuna collisione negli end-system!" << endl;
        return true;
    } else {
        EV_ERROR << "Totale collisioni: " << collisionCount << endl;
        return false;
    }
}

void TDMAScheduler::assignOffsetsToSenders() {
    EV << "Assegnazione offset alle applicazioni e switch..." << endl;

    // 1. Configura gli end-system (come prima)
    std::map<std::string, std::vector<simtime_t>> senderFragmentOffsets;
    for (const auto& slot : schedule) {
        if (slot.isEndSystemTx && !slot.senderModule.empty()) {
            senderFragmentOffsets[slot.senderModule].push_back(slot.offset);
        }
    }

    for (const auto& pair : senderFragmentOffsets) {
        const std::string& senderModulePath = pair.first;
        const std::vector<simtime_t>& offsets = pair.second;

        cModule *senderModule = getModuleByPath(senderModulePath.c_str());
        if (!senderModule) {
            EV_WARN << "Modulo non trovato: " << senderModulePath << endl;
            continue;
        }

        std::vector<simtime_t> sortedOffsets = offsets;
        std::sort(sortedOffsets.begin(), sortedOffsets.end());

        std::stringstream ss;
        for (size_t i = 0; i < sortedOffsets.size(); ++i) {
            ss << sortedOffsets[i].dbl();
            if (i < sortedOffsets.size() - 1) ss << ",";
        }

        senderModule->par("tdmaOffsets").setStringValue(ss.str());
        EV << "Configurato " << senderModulePath << " con " << sortedOffsets.size() << " slot." << endl;
    }

    // 2. ✅ NUOVO: Configura gli switch con i loro slot TDMA
    struct SwitchSlotInfo {
        simtime_t offset;
        std::string flowName;
        int fragmentNumber;
    };
    
    std::map<std::string, std::vector<SwitchSlotInfo>> switchSlots;
    
    for (const auto& slot : schedule) {
        if (!slot.isEndSystemTx && !slot.switchNode.empty() && slot.switchPort >= 0) {
            std::string portKey = slot.switchNode + ".eth[" + std::to_string(slot.switchPort) + "]";
            
            SwitchSlotInfo slotInfo;
            slotInfo.offset = slot.offset;
            slotInfo.flowName = slot.flowName;
            slotInfo.fragmentNumber = slot.fragmentNumber;
            
            switchSlots[portKey].push_back(slotInfo);
        }
    }
    
    int configuredSwitchPorts = 0;
    
    for (const auto& pair : switchSlots) {
        const std::string& portPath = pair.first;
        const std::vector<SwitchSlotInfo>& slots = pair.second;
        
        cModule *portModule = getModuleByPath(portPath.c_str());
        if (!portModule) {
            EV_WARN << "Porta switch non trovata: " << portPath << endl;
            continue;
        }
        
        // Ordina slot per tempo
        std::vector<SwitchSlotInfo> sortedSlots = slots;
        std::sort(sortedSlots.begin(), sortedSlots.end(),
            [](const SwitchSlotInfo &a, const SwitchSlotInfo &b) {
                return a.offset < b.offset;
            });
        
        // Crea stringa: "offset1:flowName1:fragNum1,offset2:flowName2:fragNum2,..."
        std::stringstream ss;
        for (size_t i = 0; i < sortedSlots.size(); ++i) {
            ss << sortedSlots[i].offset.dbl() << ":" 
               << sortedSlots[i].flowName << ":" 
               << sortedSlots[i].fragmentNumber;
            if (i < sortedSlots.size() - 1) ss << ",";
        }
        
        portModule->par("enableTDMA").setBoolValue(true);
        portModule->par("tdmaSlots").setStringValue(ss.str());
        
        configuredSwitchPorts++;
        EV << "Configurato " << portPath << " con " << sortedSlots.size() << " slot TDMA" << endl;
    }

    EV << "Configurati " << senderFragmentOffsets.size() << " sender e " 
       << configuredSwitchPorts << " porte switch." << endl;
}