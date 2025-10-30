#include "TDMAScheduler.h"
#include <cmath>

Define_Module(TDMAScheduler);

void TDMAScheduler::initialize() {
    hyperperiod = par("hyperperiod");
    datarate = par("datarate").doubleValue();
    overhead = par("ethernetOverhead").intValue();

    EV << "=== TDMA Scheduler Initialization ===" << endl;
    EV << "Hyperperiod: " << hyperperiod << endl;
    EV << "Datarate: " << datarate << " bps" << endl;

    // Step 1: Definisci tutti i flussi
    defineFlows();

    // Step 2: Calcola tempi di trasmissione
    calculateTransmissionTimes();

    // Step 3: Genera lo schedule TDMA
    generateSchedule();

    // Step 4: Verifica assenza collisioni
    if (!checkCollisions()) {
        error("TDMA Schedule ha collisioni! Impossibile continuare.");
    }

    // Step 5: Assegna offset alle applicazioni sender
    assignOffsetsToSenders();

    EV << "=== TDMA Schedule completato con successo ===" << endl;
    EV << "Totale slot schedulati: " << schedule.size() << endl;
}

void TDMAScheduler::handleMessage(cMessage *msg) {
    // Lo scheduler non riceve messaggi durante la simulazione
    error("TDMAScheduler non dovrebbe ricevere messaggi");
}

void TDMAScheduler::defineFlows() {
    EV << "Definizione flussi..." << endl;

    // Flow 1: LD1 → CU (LiDAR)
    flows.push_back(
            { "flow1_LD1_to_CU", "LD1.senderApp[0]", SimTime(0.0014, SIMTIME_S), // 1.4ms
            1300,  // payload
                    1,     // burstSize
                    3,     // priority (3° per periodo)
                    0      // txTime (calcolato dopo)
            });

    // Flow 1b: LD2 → CU (LiDAR 2)
    flows.push_back(
            { "flow1b_LD2_to_CU", "LD2.senderApp[0]", SimTime(0.0014,
                    SIMTIME_S), 1300, 1, 3, 0 });

    // Flow 2: ME → S1 (Audio speaker 1)
    flows.push_back(
            { "flow2_ME_to_S1", "ME.senderApp[0]", SimTime(0.00025, SIMTIME_S), // 250µs
            80, 1, 1,  // Priorità massima (periodo più basso)
                    0 });

    // Flow 2: ME → S2 (Audio speaker 2)
    flows.push_back(
            { "flow2_ME_to_S2", "ME.senderApp[1]", SimTime(0.00025, SIMTIME_S),
                    80, 1, 1, 0 });

    // Flow 2: ME → S3 (Audio speaker 3)
    flows.push_back(
            { "flow2_ME_to_S3", "ME.senderApp[2]", SimTime(0.00025, SIMTIME_S),
                    80, 1, 1, 0 });

    // Flow 2: ME → S4 (Audio speaker 4)
    flows.push_back(
            { "flow2_ME_to_S4", "ME.senderApp[3]", SimTime(0.00025, SIMTIME_S),
                    80, 1, 1, 0 });

    // Flow 3: USS1 → CU (Ultrasuoni 1)
    flows.push_back(
            { "flow3_USS1_to_CU", "USS1.senderApp[0]", SimTime(0.1, SIMTIME_S), // 100ms
            188, 1, 8,  // Priorità bassa (periodo alto)
                    0 });

    // Flow 3: USS2, USS3, USS4 (simili a USS1)
    flows.push_back(
            { "flow3_USS2_to_CU", "USS2.senderApp[0]", SimTime(0.1, SIMTIME_S),
                    188, 1, 8, 0 });
    flows.push_back(
            { "flow3_USS3_to_CU", "USS3.senderApp[0]", SimTime(0.1, SIMTIME_S),
                    188, 1, 8, 0 });
    flows.push_back(
            { "flow3_USS4_to_CU", "USS4.senderApp[0]", SimTime(0.1, SIMTIME_S),
                    188, 1, 8, 0 });

    // Flow 4: CU → HU (Display, 7 frammenti)
    flows.push_back(
            { "flow4_CU_to_HU", "CU.senderApp[0]", SimTime(0.01, SIMTIME_S), // 10ms
            1500,  // Ogni frammento
                    7,     // 7 frammenti
                    4, 0 });

    // Flow 5: FC → HU (Camera frontale, 119 frammenti)
    flows.push_back(
            { "flow5_FC_to_HU", "FC.senderApp[0]", SimTime(0.01666, SIMTIME_S), // 16.66ms
            1500, 119,  // 119 frammenti
                    5, 0 });

    // Flow 6: ME → RSE (Streaming video, 119 frammenti)
    flows.push_back(
            { "flow6_ME_to_RSE", "ME.senderApp[4]", SimTime(0.03333, SIMTIME_S), // 33.33ms
            1500, 119, 6, 0 });

    // Flow 7: TLM → HU (Telematica 1)
    flows.push_back(
            { "flow7_TLM_to_HU", "TLM.senderApp[0]", SimTime(0.000625,
                    SIMTIME_S),  // 625µs
            600, 1, 2,  // Priorità alta
                    0 });

    // Flow 7: TLM → CU (Telematica 2)
    flows.push_back(
            { "flow7_TLM_to_CU", "TLM.senderApp[1]", SimTime(0.000625,
                    SIMTIME_S), 600, 1, 2, 0 });

    // Flow 8: RC → HU (Retrocamera, 119 frammenti)
    flows.push_back(
            { "flow8_RC_to_HU", "RC.senderApp[0]", SimTime(0.03333, SIMTIME_S),
                    1500, 119, 7, 0 });

    EV << "Totale flussi definiti: " << flows.size() << endl;
}

bool TDMAScheduler::checkCollisions() {
    EV << "Verifica collisioni..." << endl;

    // Ordina per offset
    std::sort(schedule.begin(), schedule.end(),
            [](const TransmissionSlot &a, const TransmissionSlot &b) {
                return a.offset < b.offset;
            });

    // Controlla sovrapposizioni
    for (size_t i = 0; i < schedule.size() - 1; i++) {
        TransmissionSlot &current = schedule[i];
        TransmissionSlot &next = schedule[i + 1];

        // Trova il flusso corrispondente per ottenere txTime
        auto it = std::find_if(flows.begin(), flows.end(),
                [&current](const FlowDescriptor &f) {
                    return f.senderModule == current.senderModule;
                });

        if (it == flows.end())
            continue;

        simtime_t endTime = current.offset + it->txTime
                + SimTime(96, SIMTIME_NS);

        if (endTime > next.offset) {
            EV_ERROR << "COLLISIONE RILEVATA!" << endl;
            EV_ERROR << "Slot " << i << " (" << current.flowName
                            << ") termina a " << endTime << endl;
            EV_ERROR << "Slot " << (i + 1) << " (" << next.flowName
                            << ") inizia a " << next.offset << endl;
            return false;
        }
    }

    EV << "Nessuna collisione rilevata!" << endl;
    return true;
}

void TDMAScheduler::assignOffsetsToSenders() {
    EV << "Assegnazione offset alle applicazioni sender..." << endl;

    // Raggruppa slot per sender module
    std::map<std::string, simtime_t> senderOffsets;

    for (auto &slot : schedule) {
        if (senderOffsets.find(slot.senderModule) == senderOffsets.end()) {
            // Primo slot di questo sender = assegna offset
            senderOffsets[slot.senderModule] = slot.offset;
        }
    }

    // Imposta i parametri tdmaOffset nelle applicazioni
    for (auto &entry : senderOffsets) {
        std::string modulePath = std::string("^.") + entry.first;
        cModule *senderModule = getModuleByPath(modulePath.c_str());

        if (senderModule == nullptr) {
            EV_WARN << "Modulo sender non trovato: " << modulePath << endl;
            continue;
        }

        // Imposta il parametro tdmaOffset
        senderModule->par("tdmaOffset").setDoubleValue(entry.second.dbl());

        EV << "Assegnato offset " << entry.second << " a " << entry.first
                  << endl;
    }
}

void TDMAScheduler::calculateTransmissionTimes() {
    EV << "Calcolo tempi di trasmissione..." << endl;

    for (auto &flow : flows) {
        flow.txTime = calculateTxTime(flow.payloadSize);
        EV << "Flow " << flow.flowName << ": T_tx = " << flow.txTime << endl;
    }
}

simtime_t TDMAScheduler::calculateTxTime(int payloadSize) {
    // T_tx = (payload + overhead) * 8 / datarate
    uint64_t totalBytes = payloadSize + overhead;
    uint64_t totalBits = totalBytes * 8;

    // Converti in secondi
    double txTimeSec = (double) totalBits / (double) datarate;
    return SimTime(txTimeSec, SIMTIME_S);
}

int TDMAScheduler::calculateTransmissionsInHyperperiod(simtime_t period) {
    return (int) (hyperperiod / period);
}

// Sostituisci la vecchia funzione generateSchedule con questa versione definitiva
void TDMAScheduler::generateSchedule()
{
    EV << "Generazione schedule TDMA con Rate Monotonic (Preemptive a livello di frammento)..." << endl;

    struct Job {
        std::string flowName;
        std::string senderModule;
        simtime_t releaseTime;
        simtime_t deadline;
        simtime_t txTime;
        int burstSize;
        simtime_t period;
        int fragmentsScheduled = 0; // Contatore per i frammenti di questo job
    };

    std::vector<Job> allJobs;
    int totalFragmentsInHyperperiod = 0;
    // 1. Genera tutte le istanze dei job nell'iperperiodo
    for (const auto& flow : flows) {
        int numInstances = calculateTransmissionsInHyperperiod(flow.period);
        totalFragmentsInHyperperiod += numInstances * flow.burstSize;
        for (int i = 0; i < numInstances; ++i) {
            simtime_t release = i * flow.period;
            allJobs.push_back({
                flow.flowName,
                flow.senderModule,
                release,
                release + flow.period,
                flow.txTime,
                flow.burstSize,
                flow.period
            });
        }
    }

    simtime_t currentTime = 0;
    int scheduledFragmentCount = 0;

    // 2. Loop principale: continua finché non abbiamo schedulato tutti i frammenti
    while (scheduledFragmentCount < totalFragmentsInHyperperiod) {
        std::vector<Job*> readyQueue;
        // Costruisci la coda dei pronti: job rilasciati che hanno ancora frammenti da inviare
        for (auto& job : allJobs) {
            if (job.releaseTime <= currentTime && job.fragmentsScheduled < job.burstSize) {
                readyQueue.push_back(&job);
            }
        }

        // Se nessun job è pronto, avanza il tempo al prossimo evento
        if (readyQueue.empty()) {
            simtime_t nextReleaseTime = -1;
            for (auto& job : allJobs) {
                if (job.fragmentsScheduled < job.burstSize) { // Considera solo job non completati
                    if (nextReleaseTime < 0 || job.releaseTime < nextReleaseTime) {
                        nextReleaseTime = job.releaseTime;
                    }
                }
            }
            if (nextReleaseTime > currentTime) {
                currentTime = nextReleaseTime;
            }
            continue;
        }

        // Ordina la coda dei pronti per PRIORITÀ (Rate Monotonic)
        std::sort(readyQueue.begin(), readyQueue.end(),
            [](const Job* a, const Job* b) {
                return a->period < b->period;
            });

        // Prendi il job con la priorità più alta
        Job* jobToSchedule = readyQueue.front();

        // Schedula UN SOLO FRAMMENTO di questo job
        TransmissionSlot slot;
        slot.flowName = jobToSchedule->flowName;
        slot.senderModule = jobToSchedule->senderModule;
        slot.offset = currentTime;
        slot.fragmentNumber = jobToSchedule->fragmentsScheduled + 1;

        simtime_t finishTime = currentTime + jobToSchedule->txTime;

        // Verifica la deadline. Se fallisce qui, lo schedule è impossibile.
        if (finishTime > jobToSchedule->deadline) {
            error("DEADLINE MISS! Flow %s (istanza a t=%s) non rispetta la deadline! Fine frammento %d: %s, Deadline: %s",
                  jobToSchedule->flowName.c_str(), jobToSchedule->releaseTime.str().c_str(),
                  slot.fragmentNumber, finishTime.str().c_str(), jobToSchedule->deadline.str().c_str());
        }

        schedule.push_back(slot);
        jobToSchedule->fragmentsScheduled++;
        scheduledFragmentCount++;

        // Avanza il tempo della timeline
        currentTime = finishTime + SimTime(96, SIMTIME_NS); // Aggiungi IFG
    }

    EV << "Schedule generato con successo!" << endl;
    EV << "Totale slot schedulati: " << schedule.size() << endl;
    EV << "Tempo finale dello schedule: " << currentTime << endl;
}