// Implementazione scheduler
#include "TDMAScheduler.h"
#include "../common/Constants.h"
#include <algorithm>
#include <sstream>
#include <set>
#include <map>
#include <iostream>

Define_Module(TDMAScheduler);

// Prenotazione di un link per un intervallo temporale
struct LinkReservation {
    simtime_t start;
    simtime_t end;
};

// Tabella globale delle prenotazioni link
static std::map<std::string, std::vector<LinkReservation>> linkTable;

// Job da schedulare: un frammento di un flusso in una specifica istanza
struct Job {
    std::string flowId;
    std::string srcNode;
    simtime_t releaseTime;
    simtime_t deadline;
    simtime_t txDuration;
    int priority;
    int fragmentIndex;
    std::vector<std::string> destinations;
};

// Verifica disponibilita link in [start, start+duration+guard]
static bool isLinkFree(const std::string& linkId, simtime_t start, simtime_t duration, simtime_t guard) {
    simtime_t reqEnd = start + duration + guard;
    if (linkTable.find(linkId) == linkTable.end()) return true;
    for (const auto& res : linkTable[linkId]) {
        if (start < res.end && res.start < reqEnd) return false;
    }
    return true;
}

static void reserveLink(const std::string& linkId, simtime_t start, simtime_t duration, simtime_t guard) {
    linkTable[linkId].push_back({start, start + duration + guard});
}

void TDMAScheduler::initialize() {
    hyperperiod = par("hyperperiod");
    datarate = par("datarate").doubleValue();
    guardTime = par("guardTime").doubleValue();

    try { tdma::setGuardTime(guardTime); } catch (...) {}

    std::cout << "------------------------------------------------" << std::endl;
    std::cout << "TDMA SCHEDULER: Inizio calcolo tabella..." << std::endl;
    std::cout << "------------------------------------------------" << std::endl;


    
    flows.clear();
    schedule.clear();
    linkTable.clear();

    defineFlows();
    generateOptimizedSchedule();
    configureSenders();
    configureSwitches();
    
    std::cout << "------------------------------------------------" << std::endl;
    std::cout << "TDMA SCHEDULER: Calcolo completato" << std::endl;
    std::cout << "------------------------------------------------" << std::endl;

}

// Definizione degli 8 flussi da specifica progetto
void TDMAScheduler::defineFlows() {
    // Flow 1: LiDAR -> CU (safety-critical, 1.4ms, 1300B)
    flows.push_back({"flow1_LD1", "LD1", "CU", "00:00:00:00:00:03", "00:00:00:00:00:07", 
        SimTime(1.4, SIMTIME_MS), 1300, tdma::PRIO_CRITICAL_SAFETY, 
        {"LD1", "switch1", "switch2", "CU"}, SimTime(0), false, 1});
    flows.push_back({"flow1_LD2", "LD2", "CU", "00:00:00:00:00:0A", "00:00:00:00:00:07", 
        SimTime(1.4, SIMTIME_MS), 1300, tdma::PRIO_CRITICAL_SAFETY, 
        {"LD2", "switch2", "CU"}, SimTime(0), false, 1});

    // Flow 2: Audio multicast ME -> 4 speaker (250us, 80B)
    flows.push_back({"flow2_multicast", "ME", "S1,S2,S3,S4", "00:00:00:00:00:0B", "multicast", 
        SimTime(250, SIMTIME_US), 80, tdma::PRIO_INFOTAINMENT, 
        {"ME", "switch3"}, SimTime(0), false, 1});

    // Flow 3: Sensori parcheggio US -> CU (100ms, 188B)
    flows.push_back({"flow3_US1", "US1", "CU", "00:00:00:00:00:02", "00:00:00:00:00:07", 
        SimTime(100, SIMTIME_MS), 188, tdma::PRIO_BEST_EFFORT, 
        {"US1", "switch1", "switch2", "CU"}, SimTime(0), false, 1});
    flows.push_back({"flow3_US2", "US2", "CU", "00:00:00:00:00:09", "00:00:00:00:00:07", 
        SimTime(100, SIMTIME_MS), 188, tdma::PRIO_BEST_EFFORT, 
        {"US2", "switch2", "CU"}, SimTime(0), false, 1});
    flows.push_back({"flow3_US3", "US3", "CU", "00:00:00:00:00:10", "00:00:00:00:00:07", 
        SimTime(100, SIMTIME_MS), 188, tdma::PRIO_BEST_EFFORT, 
        {"US3", "switch4", "switch2", "CU"}, SimTime(0), false, 1});
    flows.push_back({"flow3_US4", "US4", "CU", "00:00:00:00:00:0C", "00:00:00:00:00:07", 
        SimTime(100, SIMTIME_MS), 188, tdma::PRIO_BEST_EFFORT, 
        {"US4", "switch3", "switch1", "switch2", "CU"}, SimTime(0), false, 1});

    // Flow 4: Control CU -> HU frammentato (10ms, 10500B -> 7 frammenti)
    flows.push_back({"flow4", "CU", "HU", "00:00:00:00:00:07", "00:00:00:00:00:06", 
        SimTime(10, SIMTIME_MS), 10500, tdma::PRIO_CONTROL, 
        {"CU", "switch2", "switch1", "HU"}, SimTime(0), true, 7});

    // Flow 5: Camera frontale CM1 -> HU (16.66ms, 178500B -> 119 frammenti, 60FPS)
    flows.push_back({"flow5", "CM1", "HU", "00:00:00:00:00:04", "00:00:00:00:00:06", 
        SimTime(16.66, SIMTIME_MS), 178500, tdma::PRIO_CRITICAL_SAFETY, 
        {"CM1", "switch1", "HU"}, SimTime(0), true, 119});

    // Flow 6: Video multicast ME -> RS1,RS2 (33.33ms, 178500B, 30FPS)
    flows.push_back({"flow6_multicast", "ME", "RS1,RS2", "00:00:00:00:00:0B", "multicast", 
        SimTime(33.33, SIMTIME_MS), 178500, tdma::PRIO_INFOTAINMENT, 
        {"ME", "switch3"}, SimTime(0), true, 119});

    // Flow 7: Telematics TLM -> HU e CU (625us, 600B)
    flows.push_back({"flow7_HU", "TLM", "HU", "00:00:00:00:00:01", "00:00:00:00:00:06", 
        SimTime(625, SIMTIME_US), 600, tdma::PRIO_CONTROL, 
        {"TLM", "switch1", "HU"}, SimTime(0), false, 1});
    flows.push_back({"flow7_CU", "TLM", "CU", "00:00:00:00:00:01", "00:00:00:00:00:07", 
        SimTime(625, SIMTIME_US), 600, tdma::PRIO_CONTROL, 
        {"TLM", "switch1", "switch2", "CU"}, SimTime(0), false, 1});

    // Flow 8: Rear camera RC -> HU (33.33ms, 178000B, 30FPS)
    flows.push_back({"flow8", "RC", "HU", "00:00:00:00:00:0F", "00:00:00:00:00:06", 
        SimTime(33.33, SIMTIME_MS), 178000, tdma::PRIO_CRITICAL_SAFETY, 
        {"RC", "switch4", "switch2", "switch1", "HU"}, SimTime(0), true, 119});
}

// Algoritmo EDF: genera job, ordina per deadline, schedula con verifica link
void TDMAScheduler::generateOptimizedSchedule() {
    linkTable.clear();
    std::vector<Job> jobs;

    // Generazione job per ogni flusso
    for (auto& flow : flows) {
        int payload = flow.isFragmented ? tdma::MTU_BYTES : flow.payload;
        simtime_t txTime = calculateTxTime(payload);
        flow.txTime = txTime;

        int numTransmissions = std::max(1, (int)(hyperperiod / flow.period));
        if (hyperperiod < flow.period) numTransmissions = 1;

        // Parse destinazioni multicast
        std::vector<std::string> destinations;
        if (flow.dst.find(',') != std::string::npos) {
            std::stringstream ss(flow.dst);
            std::string d;
            while(std::getline(ss, d, ',')) destinations.push_back(d);
        } else {
            destinations.push_back(flow.dst);
        }

        // Crea job per ogni istanza e frammento
        for (int i = 0; i < numTransmissions; i++) {
            simtime_t release = i * flow.period;
            simtime_t deadline = (i + 1) * flow.period;
            int frags = flow.isFragmented ? flow.fragmentCount : 1;

            for (int k = 0; k < frags; k++) {
                Job job;
                job.flowId = flow.id;
                job.srcNode = flow.src;
                job.releaseTime = release;
                job.deadline = deadline;
                job.txDuration = txTime;
                job.priority = flow.priority;
                job.fragmentIndex = k;
                job.destinations = destinations;
                jobs.push_back(job);
            }
        }
    }

    // Ordinamento EDF: deadline crescente, priorita come tie-breaker
    std::sort(jobs.begin(), jobs.end(), [](const Job& a, const Job& b) {
        if (a.deadline != b.deadline) return a.deadline < b.deadline;
        if (a.priority != b.priority) return a.priority < b.priority;
        return a.releaseTime < b.releaseTime;
    });

    int totalJobs = jobs.size();
    EV << "Jobs totali da schedulare: " << totalJobs << endl;
    std::cout << "Jobs da schedulare: " << totalJobs << std::endl;

    // Scheduling con progress bar
    int processed = 0;
    int lastPercent = -1;

    for (const auto& job : jobs) {
        processed++;
        int percent = (processed * 100) / totalJobs;
        if (percent % 10 == 0 && percent != lastPercent) {
            std::cout << "Scheduling... " << percent << "%" << std::endl;
            lastPercent = percent;
        }

        simtime_t t = job.releaseTime;
        bool scheduled = false;
        
        // Cerca primo slot libero su tutti i link del path
        while (!scheduled) {
            if (t > hyperperiod * 1.5) break;

            bool pathFree = true;
            std::map<std::string, simtime_t> linkArrivals;

            // Calcola tempo di arrivo su ogni link per ogni destinazione
            for (const auto& dest : job.destinations) {
                std::vector<std::string> path = getPathTo(job.srcNode, dest);
                simtime_t hopTime = t;

                for (size_t i = 0; i < path.size() - 1; i++) {
                    std::string u = path[i];
                    std::string v = path[i+1];
                    std::string linkId = u + "->" + v;
                    if (i > 0) hopTime += tdma::getSwitchDelay();
                    if (linkArrivals.find(linkId) == linkArrivals.end()) 
                        linkArrivals[linkId] = hopTime;
                    hopTime += job.txDuration + tdma::getPropagationDelay();
                }
            }

            // Verifica disponibilita tutti i link
            for (const auto& entry : linkArrivals) {
                if (!isLinkFree(entry.first, entry.second, job.txDuration, guardTime)) {
                    pathFree = false;
                    break;
                }
            }

            if (pathFree) {
                scheduled = true;
                schedule.push_back({job.flowId, job.srcNode, t, job.txDuration, SLOT_SENDER});
                
                // Riserva link e crea slot switch
                for (const auto& entry : linkArrivals) {
                    reserveLink(entry.first, entry.second, job.txDuration, guardTime);
                    std::string senderNode = entry.first.substr(0, entry.first.find("->"));
                    if (senderNode.find("switch") != std::string::npos) {
                        schedule.push_back({job.flowId, senderNode, entry.second, 
                            job.txDuration, SLOT_SWITCH});
                    }
                }
            } else {
                t += 1e-6; // Retry 1us dopo
            }
        }
    }
}

simtime_t TDMAScheduler::calculateTxTime(int payloadBytes) {
    int totalBytes = payloadBytes + tdma::ETHERNET_OVERHEAD;
    return SimTime((double)(totalBytes * 8) / datarate, SIMTIME_S);
}

// Inietta slot calcolati nei parametri dei TDMASenderApp
void TDMAScheduler::configureSenders() {
    std::map<std::string, int> senderIndices;
    
    for (const auto& flow : flows) {
        int senderIdx = senderIndices[flow.src]++;
        std::string path = flow.src + ".senderApp[" + std::to_string(senderIdx) + "]";
        cModule* sender = getModuleByPath(path.c_str());
        if (!sender) continue;

        // Raccogli e ordina slot per questo flusso
        std::stringstream offsets;
        bool first = true;
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
        }
        
        if (!flowSlots.empty()) {
            sender->par("tdmaSlots").setStringValue(offsets.str());
            sender->par("txDuration").setDoubleValue(flow.txTime.dbl());
            sender->par("flowId").setStringValue(flow.id);
            sender->par("srcAddr").setStringValue(flow.srcMac);
            sender->par("payloadSize").setIntValue(flow.isFragmented ? tdma::MTU_BYTES : flow.payload);
            sender->par("burstSize").setIntValue(flow.isFragmented ? flow.fragmentCount : 1);
            
            if (flow.dst.find(',') != std::string::npos) {
                sender->par("dstAddr").setStringValue("multicast");
                int numDest = std::count(flow.dst.begin(), flow.dst.end(), ',') + 1;
                sender->par("numDestinations").setIntValue(numDest);
            } else {
                sender->par("dstAddr").setStringValue(flow.dstMac);
            }

            sender->par("hyperperiod").setDoubleValue(hyperperiod.dbl());
        }
    }
}

// Configura MAC table di tutti gli switch con routing statico
void TDMAScheduler::configureSwitches() {
    std::map<std::string, std::map<std::string, std::string>> switchTables;
    
    auto addEntry = [&](std::string sw, std::string mac, int port) {
        std::string pStr = std::to_string(port);
        if (switchTables[sw][mac].empty()) switchTables[sw][mac] = pStr;
        else if (switchTables[sw][mac].find(pStr) == std::string::npos) 
            switchTables[sw][mac] += ";" + pStr;
    };

    // Switch 1 - Anteriore sinistro
    addEntry("switch1", "00:00:00:00:00:05", 0); // S1
    addEntry("switch1", "00:00:00:00:00:03", 1); // LD1
    addEntry("switch1", "00:00:00:00:00:07", 2); // CU via sw2
    addEntry("switch1", "00:00:00:00:00:0A", 2); // LD2 via sw2
    addEntry("switch1", "00:00:00:00:00:08", 2); // S2 via sw2
    addEntry("switch1", "00:00:00:00:00:0B", 3); // ME via sw3
    addEntry("switch1", "00:00:00:00:00:0D", 3); // S3 via sw3
    addEntry("switch1", "00:00:00:00:00:11", 2); // S4 via sw2->sw4
    addEntry("switch1", "00:00:00:00:00:06", 4); // HU
    addEntry("switch1", "00:00:00:00:00:02", 5); // US1
    addEntry("switch1", "00:00:00:00:00:09", 2); // US2 via sw2
    addEntry("switch1", "00:00:00:00:00:10", 2); // US3 via sw2->sw4
    addEntry("switch1", "00:00:00:00:00:0C", 3); // US4 via sw3
    addEntry("switch1", "00:00:00:00:00:04", 6); // CM1
    addEntry("switch1", "00:00:00:00:00:01", 7); // TLM
    addEntry("switch1", "00:00:00:00:00:12", 2); // RS1 via sw2->sw4
    addEntry("switch1", "00:00:00:00:00:0E", 3); // RS2 via sw3
    addEntry("switch1", "00:00:00:00:00:0F", 2); // RC via sw2->sw4
    addEntry("switch1", "multicast", 0);
    addEntry("switch1", "multicast", 2);

    // Switch 2 - Anteriore destro
    addEntry("switch2", "00:00:00:00:00:08", 1); // S2
    addEntry("switch2", "00:00:00:00:00:0A", 2); // LD2
    addEntry("switch2", "00:00:00:00:00:07", 3); // CU
    addEntry("switch2", "00:00:00:00:00:03", 0); // LD1 via sw1
    addEntry("switch2", "00:00:00:00:00:05", 0); // S1 via sw1
    addEntry("switch2", "00:00:00:00:00:0B", 0); // ME via sw1->sw3
    addEntry("switch2", "00:00:00:00:00:0D", 0); // S3 via sw1->sw3
    addEntry("switch2", "00:00:00:00:00:11", 4); // S4 via sw4
    addEntry("switch2", "00:00:00:00:00:06", 0); // HU via sw1
    addEntry("switch2", "00:00:00:00:00:02", 0); // US1 via sw1
    addEntry("switch2", "00:00:00:00:00:09", 5); // US2
    addEntry("switch2", "00:00:00:00:00:10", 4); // US3 via sw4
    addEntry("switch2", "00:00:00:00:00:0C", 0); // US4 via sw1->sw3
    addEntry("switch2", "00:00:00:00:00:04", 0); // CM1 via sw1
    addEntry("switch2", "00:00:00:00:00:01", 0); // TLM via sw1
    addEntry("switch2", "00:00:00:00:00:12", 4); // RS1 via sw4
    addEntry("switch2", "00:00:00:00:00:0E", 0); // RS2 via sw1->sw3
    addEntry("switch2", "00:00:00:00:00:0F", 4); // RC via sw4
    addEntry("switch2", "multicast", 1);
    addEntry("switch2", "multicast", 4);

    // Switch 3 - Posteriore sinistro
    addEntry("switch3", "00:00:00:00:00:0B", 0); // ME
    addEntry("switch3", "00:00:00:00:00:0D", 2); // S3
    addEntry("switch3", "00:00:00:00:00:05", 1); // S1 via sw1
    addEntry("switch3", "00:00:00:00:00:08", 1); // S2 via sw1->sw2
    addEntry("switch3", "00:00:00:00:00:11", 3); // S4 via sw4
    addEntry("switch3", "00:00:00:00:00:03", 1); // LD1 via sw1
    addEntry("switch3", "00:00:00:00:00:0A", 1); // LD2 via sw1->sw2
    addEntry("switch3", "00:00:00:00:00:07", 1); // CU via sw1->sw2
    addEntry("switch3", "00:00:00:00:00:02", 1); // US1 via sw1
    addEntry("switch3", "00:00:00:00:00:09", 1); // US2 via sw1->sw2
    addEntry("switch3", "00:00:00:00:00:10", 3); // US3 via sw4
    addEntry("switch3", "00:00:00:00:00:0C", 4); // US4
    addEntry("switch3", "00:00:00:00:00:04", 1); // CM1 via sw1
    addEntry("switch3", "00:00:00:00:00:12", 3); // RS1 via sw4
    addEntry("switch3", "00:00:00:00:00:0E", 5); // RS2
    addEntry("switch3", "00:00:00:00:00:01", 1); // TLM via sw1
    addEntry("switch3", "00:00:00:00:00:0F", 3); // RC via sw4
    addEntry("switch3", "00:00:00:00:00:06", 1); // HU via sw1
    addEntry("switch3", "multicast", 1);
    addEntry("switch3", "multicast", 2);
    addEntry("switch3", "multicast", 3);
    addEntry("switch3", "multicast", 5);

    // Switch 4 - Posteriore destro
    addEntry("switch4", "00:00:00:00:00:11", 2); // S4
    addEntry("switch4", "00:00:00:00:00:0B", 1); // ME via sw3
    addEntry("switch4", "00:00:00:00:00:0D", 1); // S3 via sw3
    addEntry("switch4", "00:00:00:00:00:05", 0); // S1 via sw2->sw1
    addEntry("switch4", "00:00:00:00:00:08", 0); // S2 via sw2
    addEntry("switch4", "00:00:00:00:00:03", 0); // LD1 via sw2->sw1
    addEntry("switch4", "00:00:00:00:00:0A", 0); // LD2 via sw2
    addEntry("switch4", "00:00:00:00:00:07", 0); // CU via sw2
    addEntry("switch4", "00:00:00:00:00:02", 0); // US1 via sw2->sw1
    addEntry("switch4", "00:00:00:00:00:09", 0); // US2 via sw2
    addEntry("switch4", "00:00:00:00:00:10", 3); // US3
    addEntry("switch4", "00:00:00:00:00:0C", 1); // US4 via sw3
    addEntry("switch4", "00:00:00:00:00:04", 0); // CM1 via sw2->sw1
    addEntry("switch4", "00:00:00:00:00:12", 5); // RS1
    addEntry("switch4", "00:00:00:00:00:0E", 1); // RS2 via sw3
    addEntry("switch4", "00:00:00:00:00:01", 0); // TLM via sw2->sw1
    addEntry("switch4", "00:00:00:00:00:0F", 4); // RC
    addEntry("switch4", "00:00:00:00:00:06", 0); // HU via sw2->sw1
    addEntry("switch4", "multicast", 2);
    addEntry("switch4", "multicast", 5);

    // Applica configurazione agli switch
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
    }
}

// Mappa statica dei path tra nodi
std::vector<std::string> TDMAScheduler::getPathTo(const std::string& src, const std::string& dst) {
    std::map<std::pair<std::string, std::string>, std::vector<std::string>> pathMap = {
        {{"LD1", "CU"}, {"LD1", "switch1", "switch2", "CU"}},
        {{"LD2", "CU"}, {"LD2", "switch2", "CU"}},
        {{"US1", "CU"}, {"US1", "switch1", "switch2", "CU"}},
        {{"US2", "CU"}, {"US2", "switch2", "CU"}},
        {{"US3", "CU"}, {"US3", "switch4", "switch2", "CU"}},
        {{"US4", "CU"}, {"US4", "switch3", "switch1", "switch2", "CU"}},
        {{"CU", "HU"}, {"CU", "switch2", "switch1", "HU"}},
        {{"CM1", "HU"}, {"CM1", "switch1", "HU"}},
        {{"RC", "HU"}, {"RC", "switch4", "switch2", "switch1", "HU"}},
        {{"TLM", "HU"}, {"TLM", "switch1", "HU"}},
        {{"TLM", "CU"}, {"TLM", "switch1", "switch2", "CU"}},
        {{"ME", "S1"}, {"ME", "switch3", "switch1", "S1"}},
        {{"ME", "S2"}, {"ME", "switch3", "switch1", "switch2", "S2"}},
        {{"ME", "S3"}, {"ME", "switch3", "S3"}},
        {{"ME", "S4"}, {"ME", "switch3", "switch4", "S4"}},
        {{"ME", "RS1"}, {"ME", "switch3", "switch4", "RS1"}},
        {{"ME", "RS2"}, {"ME", "switch3", "RS2"}},
    };
    return pathMap[{src, dst}];
}

void TDMAScheduler::handleMessage(cMessage *msg) {
    delete msg;
}
