#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
//#include "./OVS/TupleSpaceSearch.h"
#include "ElementaryClasses.h"
#include "./HybridTSS/HybridTSS.h"
#include "./CutTSS/CutTSS.h"
using namespace std;

string ruleFile, packetFile;
FILE *fpr, *fpt;
vector<Rule> rules;
vector<Packet> packets;
std::chrono::time_point<std::chrono::steady_clock> Start, End;
std::chrono::duration<double> elapsed_seconds{};
std::chrono::duration<double,std::milli> elapsed_milliseconds{};
// randUpdate / nInsert / nDelete removed (insertion & deletion are now separate passes)
//ofstream fout("result.csv", ios::app);
ofstream fError("ErrorLog.csv", ios::app);

void testPerformance(PacketClassifier *p) {
    int nRules  = static_cast<int>(rules.size());
    int nPacket = static_cast<int>(packets.size());

    // ── Header ──────────────────────────────────────────────────────────────
    printf("\n=================================================\n");
    printf("Classifier    : %s\n", p->funName().c_str());
    printf("Dataset       : %s\n", ruleFile.c_str());
    printf("Rule Count    : %d\n", nRules);
    printf("Packet Count  : %d\n", nPacket);

    // ── Construction ────────────────────────────────────────────────────────
    Start = std::chrono::steady_clock::now();
    p->ConstructClassifier(rules);
    End   = std::chrono::steady_clock::now();
    elapsed_seconds = End - Start;
    printf("Structure Construction Time        : %f s\n", elapsed_seconds.count());

    // ── Classification (Lookup Throughput) ──────────────────────────────────
    std::chrono::duration<double> sumTime(0);
    int matchPri = -1, matchMiss = 0;
    vector<int> results(nPacket, -1);
    const int trials = 10;
    for (int i = 0; i < trials; i++) {
        Start = std::chrono::steady_clock::now();
        for (int j = 0; j < nPacket; j++) {
            matchPri     = p->ClassifyAPacket(packets[j]);
            results[j]   = nRules - 1 - matchPri;
        }
        End = std::chrono::steady_clock::now();
        sumTime += End - Start;
        for (int j = 0; j < nPacket; j++) {
            if (results[j] == nRules || packets[j][5] < results[j]) {
                cout << rules[packets[j][5]].priority << "\t" << results[j] << "\t" << packets[j][5] << endl;
                matchMiss++;
            }
        }
    }
    double avgClassifySec = sumTime.count() / trials;
    double throughputMLPS = static_cast<double>(nPacket) / (avgClassifySec * 1e6); // MLPS = Mega Lookups Per Second
    printf("  Misclassified packets            : %d / %d\n", matchMiss, nPacket * trials);
    printf("Packet Lookup Throughput           : %f MLPS\n", throughputMLPS);

    // ── Incremental Insertion ───────────────────────────────────────────────
    Start = std::chrono::steady_clock::now();
    for (const auto& r : rules) {
        p->InsertRule(r);
    }
    End = std::chrono::steady_clock::now();
    elapsed_seconds = End - Start;
    double insertMUPS = static_cast<double>(nRules) / (elapsed_seconds.count() * 1e6);
    printf("Dynamic Incremental Insertion Speed: %f MUPS\n", insertMUPS);

    // ── Incremental Deletion ────────────────────────────────────────────────
    Start = std::chrono::steady_clock::now();
    for (const auto& r : rules) {
        p->DeleteRule(r);
    }
    End = std::chrono::steady_clock::now();
    elapsed_seconds = End - Start;
    double deleteMUPS = static_cast<double>(nRules) / (elapsed_seconds.count() * 1e6);
    printf("Dynamic Incremental Deletion Speed : %f MUPS\n", deleteMUPS);

    // ── Memory ──────────────────────────────────────────────────────────────
    Memory totalBytes = p->MemSizeBytes();
    // Payload (data): rule storage estimated as nRules * RULESIZE bytes
    double payloadBytes = static_cast<double>(nRules) * RULESIZE;
    double indexBytes   = (totalBytes > payloadBytes) ? (totalBytes - payloadBytes) : 0.0;
    printf("Payload Memory Footprint (Data)    : %.3f MB\n", payloadBytes  / (1024.0 * 1024.0));
    printf("Indexing Memory Footprint (Index)  : %.3f MB\n", indexBytes    / (1024.0 * 1024.0));
    printf("Total Memory Footprint             : %.3f MB\n", totalBytes    / (1024.0 * 1024.0));

    // ── Structural info ─────────────────────────────────────────────────────
    printf("Hierarchical Structural Layers     : Not Applicable\n");
    printf("=================================================\n\n");
}

int main(int argc, char* argv[]) {
    for (int i = 0; i < argc; i++) {
        if (string(argv[i]) == "-r") {
            ruleFile = string(argv[++ i]);
            fpr = fopen(argv[i], "r");
        }
        else if (string(argv[i]) == "-p") {
            packetFile = string(argv[++ i]);
            fpt = fopen(argv[i], "r");
        }
    }
//    fout << ruleFile << ",";
    printf("\nLoading dataset: %s  (trace: %s)\n", ruleFile.c_str(), packetFile.c_str());
    rules   = loadrule(fpr);
    packets = loadpacket(fpt);

    // ---HybridTSS---Construction---
//    PacketClassifier *HT = new HybridTSS();
    PacketClassifier *TMO = new TupleMergeOnline();
    testPerformance(TMO);
    PacketClassifier *PSTSS = new PriorityTupleSpaceSearch();
    testPerformance(PSTSS);
    PacketClassifier *CT = new CutTSS();
    testPerformance(CT);

    // ---test---
//    cout <<"---------------------" << endl;
    PacketClassifier *HT = new HybridTSS();
    testPerformance(HT);
//    fout << endl;

    return 0;
}
