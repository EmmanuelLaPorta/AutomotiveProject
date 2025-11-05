#include "TDMAScheduler.h"
#include <cmath>
#include <algorithm>

Define_Module(TDMAScheduler);

void TDMAScheduler::initialize() {
    hyperperiod = par("hyperperiod");
    datarate = par("datarate").doubleValue();
    overhead = par("ethernetOverhead").intValue();

    EV << "=== TDMA Scheduler Initialization ===" << endl;
    EV << "Hyperperiod: " << hyperperiod << endl;
    EV << "Datarate: " << datarate << " bps" << endl;

    defineFlows();
    calculateTransmissionTimes();
    generateSchedule();

    if (!checkCollisions()) {
        error("TDMA Schedule ha collisioni! Impossibile continuare.");
    }

    assignOffsetsToSenders();

    EV << "=== TDMA Schedule completato con successo ===" << endl;
    EV << "Totale slot schedulati: " << schedule.size() << endl;
}

void TDMAScheduler::handleMessage(cMessage *msg) {
    error("TDMAScheduler non dovrebbe ricevere messaggi");
}

void TDMAScheduler::defineFlows() {
    EV << "Definizione flussi..." << endl;

    // Flow 2: ME → S1-S4 (Audio speaker) - PRIORITÀ 1 (massima)
    flows.push_back({
        "flow2_ME_to_S1", "ME.senderApp[0]", 
        SimTime(0.00025, SIMTIME_S), 80, 1, 1, 0
    });
    flows.push_back({
        "flow2_ME_to_S2", "ME.senderApp[1]", 
        SimTime(0.00025, SIMTIME_S), 80, 1, 1, 0
    });
    flows.push_back({
        "flow2_ME_to_S3", "ME.senderApp[2]", 
        SimTime(0.00025, SIMTIME_S), 80, 1, 1, 0
    });
    flows.push_back({
        "flow2_ME_to_S4", "ME.senderApp[3]", 
        SimTime(0.00025, SIMTIME_S), 80, 1, 1, 0
    });

    // Flow 7: TLM → HU, CU (Telematica) - PRIORITÀ 2
    flows.push_back({
        "flow7_TLM_to_HU", "TLM.senderApp[0]", 
        SimTime(0.000625, SIMTIME_S), 600, 1, 2, 0
    });
    flows.push_back({
        "flow7_TLM_to_CU", "TLM.senderApp[1]", 
        SimTime(0.000625, SIMTIME_S), 600, 1, 2, 0
    });

    // Flow 1: LD1, LD2 → CU (LiDAR) - PRIORITÀ 3
    flows.push_back({
        "flow1_LD1_to_CU", "LD1.senderApp[0]", 
        SimTime(0.0014, SIMTIME_S), 1300, 1, 3, 0
    });
    flows.push_back({
        "flow1b_LD2_to_CU", "LD2.senderApp[0]", 
        SimTime(0.0014, SIMTIME_S), 1300, 1, 3, 0
    });

    // Flow 4: CU → HU (Display, 7 frammenti) - PRIORITÀ 4
    flows.push_back({
        "flow4_CU_to_HU", "CU.senderApp[0]", 
        SimTime(0.01, SIMTIME_S), 1500, 7, 4, 0
    });

    // Flow 5: CM1 → HU (Camera frontale, 119 frammenti) - PRIORITÀ 5
    flows.push_back({
        "flow5_CM1_to_HU", "CM1.senderApp[0]", 
        SimTime(0.01666, SIMTIME_S), 1500, 119, 5, 0
    });

    // Flow 6: ME → RS1, RS2 (Streaming video, 119 frammenti) - PRIORITÀ 6
    flows.push_back({
        "flow6_ME_to_RS1", "ME.senderApp[4]", 
        SimTime(0.03333, SIMTIME_S), 1500, 119, 6, 0
    });
    flows.push_back({
        "flow6_ME_to_RS2", "ME.senderApp[5]", 
        SimTime(0.03333, SIMTIME_S), 1500, 119, 6, 0
    });

    // Flow 8: RC → HU (Retrocamera, 119 frammenti) - PRIORITÀ 7
    flows.push_back({
        "flow8_RC_to_HU", "RC.senderApp[0]", 
        SimTime(0.03333, SIMTIME_S), 1500, 119, 7, 0
    });

    // Flow 3: US1-4 → CU (Ultrasuoni) - PRIORITÀ 8 (più bassa)
    flows.push_back({
        "flow3_US1_to_CU", "US1.senderApp[0]", 
        SimTime(0.1, SIMTIME_S), 188, 1, 8, 0
    });
    flows.push_back({
        "flow3_US2_to_CU", "US2.senderApp[0]", 
        SimTime(0.1, SIMTIME_S), 188, 1, 8, 0
    });
    flows.push_back({
        "flow3_US3_to_CU", "US3.senderApp[0]", 
        SimTime(0.1, SIMTIME_S), 188, 1, 8, 0
    });
    flows.push_back({
        "flow3_US4_to_CU", "US4.senderApp[0]", 
        SimTime(0.1, SIMTIME_S), 188, 1, 8, 0
    });

    EV << "Totale flussi definiti: " << flows.size() << endl;
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
    return (int)ceil(hyperperiod.dbl() / period.dbl());
}

void TDMAScheduler::generateSchedule()
{
    EV << "=== Generazione Schedule TDMA Interleaved Rate Monotonic ===" << endl;

    // Struttura per tracciare i job
    struct Job {
        std::string flowName;
        std::string senderModule;
        int instanceNumber;
        simtime_t releaseTime;
        simtime_t deadline;
        simtime_t txTime;
        int burstSize;
        int fragmentsScheduled;
        int priority;
        simtime_t period;
    };

    std::vector<Job> allJobs;

    // Genera tutte le istanze dei job nell'iperperiodo
    for (const auto& flow : flows) {
        int numInstances = calculateTransmissionsInHyperperiod(flow.period);
        
        if (numInstances * flow.period > hyperperiod) {
                numInstances = (int)(hyperperiod.dbl() / flow.period.dbl());
            }

        for (int i = 0; i < numInstances; ++i) {
            simtime_t release = i * flow.period;
            simtime_t deadline = release + flow.period;
            
            allJobs.push_back({
                flow.flowName,
                flow.senderModule,
                i,
                release,
                deadline,
                flow.txTime,
                flow.burstSize,
                0,  // fragmentsScheduled
                flow.priority,
                flow.period
            });
        }
    }

    EV << "Totale istanze job: " << allJobs.size() << endl;

    // ✅ SOLUZIONE: Interleaved Scheduling
    // Invece di schedulare tutti i frammenti di un burst consecutivamente,
    // scheduliamo UN FRAMMENTO alla volta e poi ricontrolliamo la priorità
    
    simtime_t currentTime = 0;
    const simtime_t IFG = SimTime(96, SIMTIME_NS);  // Inter-Frame Gap
    const simtime_t GUARD_TIME = SimTime(500, SIMTIME_NS);  // Guard time extra
    
    int totalFragmentsScheduled = 0;
    int totalFragmentsNeeded = 0;
    
    for (const auto& job : allJobs) {
        totalFragmentsNeeded += job.burstSize;
    }

    EV << "Totale frammenti da schedulare: " << totalFragmentsNeeded << endl;

    while (totalFragmentsScheduled < totalFragmentsNeeded) {
        
        // 1. Trova tutti i job pronti
        std::vector<Job*> readyQueue;
        
        for (auto& job : allJobs) {
            if (job.releaseTime <= currentTime && job.fragmentsScheduled < job.burstSize) {
                readyQueue.push_back(&job);
            }
        }

        // 2. Se nessuno è pronto, salta al prossimo release
        if (readyQueue.empty()) {
            simtime_t nextRelease = hyperperiod;
            
            for (auto& job : allJobs) {
                if (job.fragmentsScheduled < job.burstSize && 
                    job.releaseTime > currentTime && 
                    job.releaseTime < nextRelease) {
                    nextRelease = job.releaseTime;
                }
            }
            
            if (nextRelease <= currentTime || nextRelease >= hyperperiod) {
                break;
            }
            
            currentTime = nextRelease;
            continue;
        }

        // 3. Ordina per priorità (numero più basso = priorità più alta)
        std::sort(readyQueue.begin(), readyQueue.end(),
            [](const Job* a, const Job* b) {
                if (a->priority != b->priority) {
                    return a->priority < b->priority;
                }
                // A parità di priorità, favorisci chi ha deadline più vicina
                if (a->deadline != b->deadline) {
                    return a->deadline < b->deadline;
                }
                // A parità di deadline, favorisci chi ha meno frammenti schedulati (fairness)
                return a->fragmentsScheduled < b->fragmentsScheduled;
            });

        // 4. Schedula UN SOLO FRAMMENTO del job con priorità più alta
        Job* selected = readyQueue[0];

        TransmissionSlot slot;
        slot.flowName = selected->flowName;
        slot.senderModule = selected->senderModule;
        slot.offset = currentTime;
        slot.fragmentNumber = selected->fragmentsScheduled + 1;
        slot.burstSize = selected->burstSize;
        slot.instanceNumber = selected->instanceNumber;

        simtime_t finishTime = currentTime + selected->txTime;

        // Verifica deadline
        if (finishTime > selected->deadline) {
            EV_ERROR << "DEADLINE MISS! Flow: " << selected->flowName 
                     << " (instance " << selected->instanceNumber 
                     << ", fragment " << (selected->fragmentsScheduled + 1) 
                     << "/" << selected->burstSize << ")" << endl;
            EV_ERROR << "    Release: " << selected->releaseTime 
                     << ", Deadline: " << selected->deadline 
                     << ", Finish: " << finishTime << endl;
            
            // Continua comunque (best-effort)
        }

        schedule.push_back(slot);
        selected->fragmentsScheduled++;
        totalFragmentsScheduled++;

        // 5. Avanza il tempo (trasmissione + IFG + guard time)
        currentTime = finishTime + IFG + GUARD_TIME;

        // Log per debug (solo ogni 100 frammenti per non intasare)
        if (totalFragmentsScheduled % 100 == 0) {
            EV << "Progress: " << totalFragmentsScheduled << "/" 
               << totalFragmentsNeeded << " frammenti schedulati" << endl;
        }
    }

    EV << "=== Schedule completato ===" << endl;
    EV << "Slot totali: " << schedule.size() << endl;
    EV << "Tempo finale: " << currentTime << endl;
    
    if (currentTime > hyperperiod) {
        double utilizzo = (currentTime.dbl() / hyperperiod.dbl()) * 100;
        EV_WARN << "??  ATTENZIONE: Schedule supera l'iperperiodo!" << endl;
        EV_WARN << "    Tempo necessario: " << currentTime << endl;
        EV_WARN << "    Iperperiodo: " << hyperperiod << endl;
        EV_WARN << "    Utilizzo canale: " << utilizzo << "%" << endl;
    } else {
        double utilizzo = (currentTime.dbl() / hyperperiod.dbl()) * 100;
        EV << "? Utilizzo canale: " << utilizzo << "%" << endl;
    }
}

bool TDMAScheduler::checkCollisions() {
    EV << "Verifica collisioni..." << endl;

    // Ordina per tempo
    std::sort(schedule.begin(), schedule.end(),
        [](const TransmissionSlot &a, const TransmissionSlot &b) {
            return a.offset < b.offset;
        });

    int collisionCount = 0;

    for (size_t i = 0; i < schedule.size() - 1; i++) {
        TransmissionSlot &current = schedule[i];
        TransmissionSlot &next = schedule[i + 1];

        // Trova il txTime per il flusso corrente
        auto it = std::find_if(flows.begin(), flows.end(),
            [&current](const FlowDescriptor &f) {
                return f.senderModule == current.senderModule;
            });

        if (it == flows.end()) {
            EV_WARN << "Flow non trovato per " << current.senderModule << endl;
            continue;
        }

        simtime_t endTime = current.offset + it->txTime + SimTime(96, SIMTIME_NS);

        if (endTime > next.offset) {
            collisionCount++;
            EV_ERROR << "COLLISIONE #" << collisionCount << endl;
            EV_ERROR << "   Slot " << i << ": " << current.flowName 
                     << " (frag " << current.fragmentNumber << ") termina a " << endTime << endl;
            EV_ERROR << "   Slot " << (i+1) << ": " << next.flowName 
                     << " (frag " << next.fragmentNumber << ") inizia a " << next.offset << endl;
            EV_ERROR << "   Overlap: " << (endTime - next.offset) << endl;
        }
    }

    if (collisionCount == 0) {
        EV << "✅ Nessuna collisione rilevata!" << endl;
        return true;
    } else {
        EV_ERROR << "Totale collisioni: " << collisionCount << endl;
        return false;
    }
}

void TDMAScheduler::assignOffsetsToSenders() {
    EV << "Assegnazione offset e fragmentTxTime alle applicazioni sender..." << endl;

    std::map<std::string, std::vector<simtime_t>> senderFragmentOffsets;
    std::map<std::string, simtime_t> senderTxTimes;

    // Raccogli TUTTI gli offset per ogni sender (per supportare interleaving)
    for (auto &slot : schedule) {
        senderFragmentOffsets[slot.senderModule].push_back(slot.offset);
        
        // Trova il txTime per questo sender (solo una volta)
        if (senderTxTimes.find(slot.senderModule) == senderTxTimes.end()) {
            auto it = std::find_if(flows.begin(), flows.end(),
                [&slot](const FlowDescriptor &f) {
                    return f.senderModule == slot.senderModule;
                });
            
            if (it != flows.end()) {
                senderTxTimes[slot.senderModule] = it->txTime;
            }
        }
    }

    // Imposta i parametri per ogni sender
    for (auto &entry : senderFragmentOffsets) {
        std::string modulePath = std::string("^.") + entry.first;
        cModule *senderModule = getModuleByPath(modulePath.c_str());

        if (senderModule == nullptr) {
            EV_WARN << "Modulo non trovato: " << modulePath << endl;
            continue;
        }

        // Imposta l'offset del PRIMO frammento
        simtime_t firstOffset = entry.second[0];
        senderModule->par("tdmaOffset").setDoubleValue(firstOffset.dbl());
        
        // Imposta fragmentTxTime
        if (senderTxTimes.find(entry.first) != senderTxTimes.end()) {
            simtime_t txTime = senderTxTimes[entry.first];
            senderModule->par("fragmentTxTime").setDoubleValue(txTime.dbl());
            
            EV << "✅ " << entry.first << ": offset=" << firstOffset 
               << ", txTime=" << txTime 
               << ", frammenti=" << entry.second.size() << endl;
        } else {
            EV_WARN << "txTime non trovato per " << entry.first << endl;
        }
    }

    EV << "✅ Assegnazione completata per " << senderFragmentOffsets.size() << " sender" << endl;
}
