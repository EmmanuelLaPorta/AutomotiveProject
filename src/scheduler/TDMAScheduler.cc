#include "TDMAScheduler.h"
#include <cmath>
#include <algorithm>
#include <set>

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

    // NOTA: Per i flussi multicast, definiamo una sola istanza.
    // Sarà compito dello switch replicare il pacchetto.
    // Il senderModule path (es. "ME.senderApp[0]") deve corrispondere
    // a quello configurato in omnetpp.ini con il MAC address multicast.

    // Flow 2: ME → S1-S4 (Audio speaker) - PRIORITÀ 1 (massima) - MULTICAST
    flows.push_back({
        "flow2_Audio_Multicast", "ME.senderApp[0]",
        SimTime(0.00025, SIMTIME_S), 80, 1, 1, 0, SimTime(1e-6, SIMTIME_S)
    });

    // Flow 7: TLM → HU, CU (Telematica) - PRIORITÀ 2 - MULTICAST
    flows.push_back({
        "flow7_Telematics_Multicast", "TLM.senderApp[0]",
        SimTime(0.000625, SIMTIME_S), 600, 1, 2, 0, SimTime(1e-6, SIMTIME_S)
    });

    // Flow 1: LD1, LD2 → CU (LiDAR) - PRIORITÀ 3
    flows.push_back({
        "flow1_LD1_to_CU", "LD1.senderApp[0]",
        SimTime(0.0014, SIMTIME_S), 1300, 1, 3, 0, SimTime(1e-6, SIMTIME_S)
    });
    flows.push_back({
        "flow1b_LD2_to_CU", "LD2.senderApp[0]",
        SimTime(0.0014, SIMTIME_S), 1300, 1, 3, 0, SimTime(1e-6, SIMTIME_S)
    });

    // Flow 4: CU → HU (Display, 7 frammenti) - PRIORITÀ 4
    flows.push_back({
        "flow4_CU_to_HU", "CU.senderApp[0]",
        SimTime(0.01, SIMTIME_S), 1500, 7, 4, 0, SimTime(1e-6, SIMTIME_S)
    });

    // Flow 5: CM1 → HU (Camera frontale, 119 frammenti) - PRIORITÀ 5
    flows.push_back({
        "flow5_CM1_to_HU", "CM1.senderApp[0]",
        SimTime(0.01666, SIMTIME_S), 1500, 119, 5, 0, SimTime(1e-6, SIMTIME_S)
    });

    // Flow 6: ME → RS1, RS2 (Streaming video, 119 frammenti) - PRIORITÀ 6 - MULTICAST
    flows.push_back({
        "flow6_Video_Multicast", "ME.senderApp[1]", // Usa senderApp[1] per distinguerlo da audio
        SimTime(0.03333, SIMTIME_S), 1500, 119, 6, 0, SimTime(1e-6, SIMTIME_S)
    });

    // Flow 8: RC → HU (Retrocamera, 119 frammenti) - PRIORITÀ 7
    flows.push_back({
        "flow8_RC_to_HU", "RC.senderApp[0]",
        SimTime(0.03333, SIMTIME_S), 1500, 119, 7, 0, SimTime(1e-6, SIMTIME_S)
    });

    // Flow 3: US1-4 → CU (Ultrasuoni) - PRIORITÀ 8 (più bassa)
    flows.push_back({
        "flow3_US1_to_CU", "US1.senderApp[0]",
        SimTime(0.1, SIMTIME_S), 188, 1, 8, 0, SimTime(1e-6, SIMTIME_S)
    });
    flows.push_back({
        "flow3_US2_to_CU", "US2.senderApp[0]",
        SimTime(0.1, SIMTIME_S), 188, 1, 8, 0, SimTime(1e-6, SIMTIME_S)
    });
    flows.push_back({
        "flow3_US3_to_CU", "US3.senderApp[0]",
        SimTime(0.1, SIMTIME_S), 188, 1, 8, 0, SimTime(1e-6, SIMTIME_S)
    });
    flows.push_back({
        "flow3_US4_to_CU", "US4.senderApp[0]",
        SimTime(0.1, SIMTIME_S), 188, 1, 8, 0, SimTime(1e-6, SIMTIME_S)
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
    return (int)floor(hyperperiod.dbl() / period.dbl());
}

void TDMAScheduler::generateSchedule()
{
    EV << "=== Generazione Schedule TDMA Frame-by-Frame Interleaved ===" << endl;

    // Struttura per Job singolo (UN FRAMMENTO)
    struct Job {
        std::string flowName;
        std::string senderModule;
        int burstInstanceNumber;      // Quale burst (es: burst 0, 1, 2...)
        int fragmentNumber;            // Quale frammento nel burst (1...119)
        simtime_t releaseTime;
        simtime_t deadline;
        simtime_t txTime;
        int priority;
        int totalFragments;
        simtime_t pathDelay; // Aggiunto pathDelay al Job
    };

    std::vector<Job> allJobs;

    // SOLUZIONE: Genera JOB INDIVIDUALI per ogni SINGOLO FRAMMENTO
    for (const auto& flow : flows) {
        int numBursts = calculateTransmissionsInHyperperiod(flow.period);

        for (int burstNum = 0; burstNum < numBursts; ++burstNum) {
            simtime_t burstRelease = burstNum * flow.period;
            simtime_t burstDeadline = burstRelease + flow.period;

            // Per ogni frammento nel burst, crea un job separato
            for (int fragNum = 1; fragNum <= flow.burstSize; ++fragNum) {
                allJobs.push_back({
                    flow.flowName,
                    flow.senderModule,
                    burstNum,
                    fragNum,
                    burstRelease,  // Tutti i frammenti dello stesso burst hanno stesso release
                    burstDeadline,
                    flow.txTime,
                    flow.priority,
                    flow.burstSize,
                    flow.pathDelay // Inizializza pathDelay per il Job
                });
            }
        }
    }

    EV << "Totale job (frammenti) da schedulare: " << allJobs.size() << endl;

    // ALGORITMO CORRETTO: Scheduling frame-by-frame con preemption
    simtime_t currentTime = SimTime(1e-6, SIMTIME_S);
    const simtime_t IFG = SimTime(96, SIMTIME_NS);
    const simtime_t GUARD_TIME = SimTime(1000, SIMTIME_NS);  // Aumentato a 1µs
    
    std::set<int> scheduledJobs;  // Traccia job già schedulati
    int totalJobs = allJobs.size();
    int scheduledCount = 0;

    while (scheduledCount < totalJobs) {
        // 1. Trova tutti i job PRONTI e NON ancora schedulati
        std::vector<int> readyJobIndices;
        
        for (size_t i = 0; i < allJobs.size(); ++i) {
            if (scheduledJobs.find(i) == scheduledJobs.end() &&
                allJobs[i].releaseTime <= currentTime) {
                readyJobIndices.push_back(i);
            }
        }

        // 2. Se nessuno pronto, salta al prossimo release
        if (readyJobIndices.empty()) {
            simtime_t nextRelease = hyperperiod;
            
            for (size_t i = 0; i < allJobs.size(); ++i) {
                if (scheduledJobs.find(i) == scheduledJobs.end() &&
                    allJobs[i].releaseTime > currentTime &&
                    allJobs[i].releaseTime < nextRelease) {
                    nextRelease = allJobs[i].releaseTime;
                }
            }
            
            if (nextRelease >= hyperperiod) {
                break;  // Fine scheduling
            }
            
            currentTime = nextRelease;
            continue;
        }

        // 3. Ordina per PRIORITÀ (Rate Monotonic) poi per fragmentNumber
        std::sort(readyJobIndices.begin(), readyJobIndices.end(),
            [&allJobs](int a, int b) {
                const Job& jobA = allJobs[a];
                const Job& jobB = allJobs[b];
                
                // Priorità più alta (numero minore)
                if (jobA.priority != jobB.priority) {
                    return jobA.priority < jobB.priority;
                }
                
                // Stessa priorità: favorisci stesso burst (continuità)
                if (jobA.flowName == jobB.flowName &&
                    jobA.burstInstanceNumber == jobB.burstInstanceNumber) {
                    return jobA.fragmentNumber < jobB.fragmentNumber;
                }
                
                // Deadline più vicina
                if (jobA.deadline != jobB.deadline) {
                    return jobA.deadline < jobB.deadline;
                }
                
                return jobA.fragmentNumber < jobB.fragmentNumber;
            });

        // 4. Schedula il job con priorità più alta
        int selectedIdx = readyJobIndices[0];
        const Job& selected = allJobs[selectedIdx];

        TransmissionSlot slot;
        slot.flowName = selected.flowName;
        slot.senderModule = selected.senderModule;
        // Applica la compensazione del pathDelay
        slot.offset = currentTime - selected.pathDelay;
        slot.fragmentNumber = selected.fragmentNumber;
        slot.burstSize = selected.totalFragments;
        slot.instanceNumber = selected.burstInstanceNumber;

        simtime_t finishTime = currentTime + selected.txTime;

        // Verifica deadline
        if (finishTime > selected.deadline) {
            EV_WARN << "Deadline miss: " << selected.flowName 
                    << " burst " << selected.burstInstanceNumber
                    << " frag " << selected.fragmentNumber << endl;
        }

        schedule.push_back(slot);
        scheduledJobs.insert(selectedIdx);
        scheduledCount++;

        // 5. Avanza tempo con guard time
        currentTime = finishTime + IFG + GUARD_TIME;

        // Progress log
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
    
    if (currentTime > hyperperiod) {
        EV_ERROR << "ERRORE: Schedule supera l'iperperiodo!" << endl;
        EV_ERROR << "   Necessario: " << currentTime << " vs Disponibile: " << hyperperiod << endl;
    }
}

bool TDMAScheduler::checkCollisions() {
    EV << "Verifica collisioni..." << endl;

    std::sort(schedule.begin(), schedule.end(),
        [](const TransmissionSlot &a, const TransmissionSlot &b) {
            return a.offset < b.offset;
        });

    int collisionCount = 0;

    for (size_t i = 0; i < schedule.size() - 1; i++) {
        TransmissionSlot &current = schedule[i];
        TransmissionSlot &next = schedule[i + 1];

        auto it = std::find_if(flows.begin(), flows.end(),
            [&current](const FlowDescriptor &f) {
                return f.senderModule == current.senderModule;
            });

        if (it == flows.end()) {
            continue;
        }

        simtime_t endTime = current.offset + it->txTime + SimTime(96, SIMTIME_NS);

        if (endTime > next.offset) {
            collisionCount++;
            EV_ERROR << "COLLISIONE: " << current.flowName 
                     << " → " << next.flowName 
                     << " (gap: " << (next.offset - endTime) << ")" << endl;
        }
    }

    if (collisionCount == 0) {
        EV << "Nessuna collisione!" << endl;
        return true;
    } else {
        EV_ERROR << "Totale collisioni: " << collisionCount << endl;
        return false;
    }
}

void TDMAScheduler::assignOffsetsToSenders() {
    EV << "Assegnazione offset alle applicazioni..." << endl;

    // Raggruppa tutti gli offset per ogni modulo sender
    std::map<std::string, std::vector<simtime_t>> senderFragmentOffsets;
    for (const auto& slot : schedule) {
        senderFragmentOffsets[slot.senderModule].push_back(slot.offset);
    }

    // Per ogni sender, crea la stringa di offset e passala come parametro
    for (const auto& pair : senderFragmentOffsets) {
        const std::string& senderModulePath = pair.first;
        const std::vector<simtime_t>& offsets = pair.second;

        cModule *senderModule = getModuleByPath(senderModulePath.c_str());
        if (!senderModule) {
            EV_WARN << "Modulo non trovato: " << senderModulePath << endl;
            continue;
        }

        // Ordina gli offset temporalmente
        std::vector<simtime_t> sortedOffsets = offsets;
        std::sort(sortedOffsets.begin(), sortedOffsets.end());

        // Crea una stringa con gli offset separati da virgola
        std::stringstream ss;
        for (size_t i = 0; i < sortedOffsets.size(); ++i) {
            ss << sortedOffsets[i].dbl();
            if (i < sortedOffsets.size() - 1) {
                ss << ",";
            }
        }

        // Imposta il parametro "tdmaOffsets" nel modulo TDMASenderApp
        senderModule->par("tdmaOffsets").setStringValue(ss.str());
        EV << "Configurato " << senderModulePath << " con " << sortedOffsets.size() << " slot." << endl;
    }

    EV << "Configurati " << senderFragmentOffsets.size() << " sender." << endl;
}