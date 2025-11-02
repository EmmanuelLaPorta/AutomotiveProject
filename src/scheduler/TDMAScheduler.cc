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

    // Flow 1: LD1 → CU (LiDAR)
    flows.push_back({
        "flow1_LD1_to_CU", "LD1.senderApp[0]", 
        SimTime(0.0014, SIMTIME_S), 1300, 1, 3, 0
    });

    // Flow 1b: LD2 → CU (LiDAR 2)
    flows.push_back({
        "flow1b_LD2_to_CU", "LD2.senderApp[0]", 
        SimTime(0.0014, SIMTIME_S), 1300, 1, 3, 0
    });

    // Flow 2: ME → S1-S4 (Audio speaker) - PRIORITÀ MASSIMA
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

    // Flow 3: USS1-4 → CU (Ultrasuoni)
    flows.push_back({
        "flow3_USS1_to_CU", "USS1.senderApp[0]", 
        SimTime(0.1, SIMTIME_S), 188, 1, 8, 0
    });
    flows.push_back({
        "flow3_USS2_to_CU", "USS2.senderApp[0]", 
        SimTime(0.1, SIMTIME_S), 188, 1, 8, 0
    });
    flows.push_back({
        "flow3_USS3_to_CU", "USS3.senderApp[0]", 
        SimTime(0.1, SIMTIME_S), 188, 1, 8, 0
    });
    flows.push_back({
        "flow3_USS4_to_CU", "USS4.senderApp[0]", 
        SimTime(0.1, SIMTIME_S), 188, 1, 8, 0
    });

    // Flow 4: CU → HU (Display, 7 frammenti)
    flows.push_back({
        "flow4_CU_to_HU", "CU.senderApp[0]", 
        SimTime(0.01, SIMTIME_S), 1500, 7, 4, 0
    });

    // Flow 5: FC → HU (Camera frontale, 119 frammenti)
    flows.push_back({
        "flow5_FC_to_HU", "FC.senderApp[0]", 
        SimTime(0.01666, SIMTIME_S), 1500, 119, 5, 0
    });

    // Flow 6: ME → RSE (Streaming video, 119 frammenti)
    flows.push_back({
        "flow6_ME_to_RSE", "ME.senderApp[4]", 
        SimTime(0.03333, SIMTIME_S), 1500, 119, 6, 0
    });

    // Flow 7: TLM → HU, CU (Telematica)
    flows.push_back({
        "flow7_TLM_to_HU", "TLM.senderApp[0]", 
        SimTime(0.000625, SIMTIME_S), 600, 1, 2, 0
    });
    flows.push_back({
        "flow7_TLM_to_CU", "TLM.senderApp[1]", 
        SimTime(0.000625, SIMTIME_S), 600, 1, 2, 0
    });

    // Flow 8: RC → HU (Retrocamera, 119 frammenti)
    flows.push_back({
        "flow8_RC_to_HU", "RC.senderApp[0]", 
        SimTime(0.03333, SIMTIME_S), 1500, 119, 7, 0
    });

    EV << "Totale flussi definiti: " << flows.size() << endl;
}

void TDMAScheduler::calculateTransmissionTimes() {
    EV << "Calcolo tempi di trasmissione..." << endl;

    for (auto &flow : flows) {
        flow.txTime = calculateTxTime(flow.payloadSize);
        EV << "Flow " << flow.flowName << ": T_tx = " << flow.txTime << endl;
    }
}

simtime_t TDMAScheduler::calculateTxTime(int payloadSize) {
    uint64_t totalBytes = payloadSize + overhead;
    uint64_t totalBits = totalBytes * 8;
    double txTimeSec = (double)totalBits / (double)datarate;
    return SimTime(txTimeSec, SIMTIME_S);
}

int TDMAScheduler::calculateTransmissionsInHyperperiod(simtime_t period) {
    return (int)(hyperperiod / period);
}

// ============================================================================
// SCHEDULAZIONE CORRETTA CON GESTIONE BURST
// ============================================================================
void TDMAScheduler::generateSchedule()
{
    EV << "=== Generazione Schedule TDMA con Rate Monotonic ===" << endl;

    struct Job {
        std::string flowName;
        std::string senderModule;
        int instanceNumber;           // Quale istanza del periodo (0, 1, 2...)
        simtime_t releaseTime;        // Quando questo job diventa pronto
        simtime_t deadline;           // Deadline assoluta
        simtime_t txTime;             // Tempo di trasmissione per frammento
        int burstSize;                // Numero totale di frammenti
        int fragmentsScheduled;       // Quanti frammenti abbiamo già schedulato
        int priority;                 // Per Rate Monotonic
        simtime_t period;             // Per debug
    };

    std::vector<Job> allJobs;
    int totalFragmentsNeeded = 0;

    // ===== FASE 1: Genera tutte le istanze dei job nell'iperperiodo =====
    for (const auto& flow : flows) {
        int numInstances = calculateTransmissionsInHyperperiod(flow.period);
        
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
                0,  // fragmentsScheduled inizia a 0
                flow.priority,
                flow.period
            });
            
            totalFragmentsNeeded += flow.burstSize;
        }
    }

    EV << "Totale istanze job: " << allJobs.size() << endl;
    EV << "Totale frammenti da schedulare: " << totalFragmentsNeeded << endl;

    simtime_t currentTime = 0;
    int scheduledFragmentCount = 0;
    const simtime_t IFG = SimTime(96, SIMTIME_NS);

    // ===== FASE 2: Loop principale di schedulazione =====
    while (scheduledFragmentCount < totalFragmentsNeeded) {
        
        // Costruisci la coda dei job pronti (che hanno ancora frammenti da inviare)
        std::vector<Job*> readyQueue;
        
        for (auto& job : allJobs) {
            if (job.releaseTime <= currentTime && job.fragmentsScheduled < job.burstSize) {
                readyQueue.push_back(&job);
            }
        }

        // Se nessun job è pronto, salta al prossimo release time
        if (readyQueue.empty()) {
            simtime_t nextRelease = hyperperiod;
            
            for (auto& job : allJobs) {
                if (job.fragmentsScheduled < job.burstSize) {
                    if (job.releaseTime < nextRelease && job.releaseTime > currentTime) {
                        nextRelease = job.releaseTime;
                    }
                }
            }
            
            if (nextRelease > currentTime) {
                EV << "Nessun job pronto a t=" << currentTime 
                   << ", salto a t=" << nextRelease << endl;
                currentTime = nextRelease;
                continue;
            }
            
            break;  // Fine schedulazione
        }

        // Ordina per priorità (Rate Monotonic: periodo più basso = priorità più alta)
        std::sort(readyQueue.begin(), readyQueue.end(),
            [](const Job* a, const Job* b) {
                if (a->priority != b->priority) {
                    return a->priority < b->priority;  // Priorità numerica più bassa vince
                }
                // Pareggio: scegli quello con deadline più vicina
                return a->deadline < b->deadline;
            });

        // Prendi il job con priorità più alta
        Job* selected = readyQueue[0];

        // ===== SCHEDULA UN SINGOLO FRAMMENTO =====
        TransmissionSlot slot;
        slot.flowName = selected->flowName;
        slot.senderModule = selected->senderModule;
        slot.offset = currentTime;
        slot.fragmentNumber = selected->fragmentsScheduled + 1;

        simtime_t finishTime = currentTime + selected->txTime;

        // Verifica deadline
        if (finishTime > selected->deadline) {
            error("DEADLINE MISS! Flow %s (istanza %d) non schedulabile.\n"
                  "Fragment %d/%d, Start: %s, Finish: %s, Deadline: %s",
                  selected->flowName.c_str(), 
                  selected->instanceNumber,
                  slot.fragmentNumber, 
                  selected->burstSize,
                  currentTime.str().c_str(),
                  finishTime.str().c_str(), 
                  selected->deadline.str().c_str());
        }

        // Aggiungi allo schedule
        schedule.push_back(slot);
        selected->fragmentsScheduled++;
        scheduledFragmentCount++;

        // Debug dettagliato
        if (selected->flowName.find("flow2") != std::string::npos) {
            EV << "[AUDIO] t=" << currentTime << " -> " 
               << selected->flowName << " fragment " 
               << slot.fragmentNumber << "/" << selected->burstSize << endl;
        }

        // Avanza il tempo (trasmissione + IFG)
        currentTime = finishTime + IFG;
    }

    EV << "=== Schedule completato ===" << endl;
    EV << "Slot totali: " << schedule.size() << endl;
    EV << "Tempo finale: " << currentTime << endl;
    
    if (currentTime > hyperperiod) {
        EV_WARN << "ATTENZIONE: Schedule supera l'iperperiodo! " 
                << currentTime << " > " << hyperperiod << endl;
    }
}

bool TDMAScheduler::checkCollisions() {
    EV << "Verifica collisioni..." << endl;

    std::sort(schedule.begin(), schedule.end(),
        [](const TransmissionSlot &a, const TransmissionSlot &b) {
            return a.offset < b.offset;
        });

    for (size_t i = 0; i < schedule.size() - 1; i++) {
        TransmissionSlot &current = schedule[i];
        TransmissionSlot &next = schedule[i + 1];

        auto it = std::find_if(flows.begin(), flows.end(),
            [&current](const FlowDescriptor &f) {
                return f.senderModule == current.senderModule;
            });

        if (it == flows.end()) continue;

        simtime_t endTime = current.offset + it->txTime + SimTime(96, SIMTIME_NS);

        if (endTime > next.offset) {
            EV_ERROR << "COLLISIONE! Slot " << i << " (" << current.flowName
                    << ") termina a " << endTime << ", Slot " << (i+1) 
                    << " (" << next.flowName << ") inizia a " << next.offset << endl;
            return false;
        }
    }

    EV << "Nessuna collisione rilevata!" << endl;
    return true;
}

void TDMAScheduler::assignOffsetsToSenders() {
    EV << "Assegnazione offset alle applicazioni sender..." << endl;

    std::map<std::string, simtime_t> senderOffsets;

    // Trova il primo offset per ogni sender
    for (auto &slot : schedule) {
        if (senderOffsets.find(slot.senderModule) == senderOffsets.end()) {
            senderOffsets[slot.senderModule] = slot.offset;
        }
    }

    // Imposta i parametri
    for (auto &entry : senderOffsets) {
        std::string modulePath = std::string("^.") + entry.first;
        cModule *senderModule = getModuleByPath(modulePath.c_str());

        if (senderModule == nullptr) {
            EV_WARN << "Modulo non trovato: " << modulePath << endl;
            continue;
        }

        senderModule->par("tdmaOffset").setDoubleValue(entry.second.dbl());
        EV << "Offset " << entry.second << " -> " << entry.first << endl;
    }
}