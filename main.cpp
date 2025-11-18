#include <iostream>
#include <zip.h>
#include <fstream>
#include <sys/stat.h>
#include <filesystem>
#include <random>
#include <vector>
#include <chrono>
#include "gtfs.h"
#include "raptor.h"


namespace fs = std::filesystem;
using namespace std;

string seconds_to_time(int sec) {
    int h = sec / 3600;
    int m = (sec % 3600) / 60;
    int s = sec % 60;
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", h, m, s);
    return string(buffer);
}

int main(int argc, char* argv[]) {
    // Make sure to unzip gtfs zip
    // const char* gtfs_zip = "gtfs-data.zip";
    // const string out_folder = "gtfs-data/";

    int iterations = 500;

    if (argc >= 2) {
        try {
            iterations = stoi(argv[1]);
        } catch (...) {
            iterations = 500;
        }
    }

    std::string filename = "raptor_results_" + std::to_string(iterations) + ".txt";
    ofstream fout(filename);

    if (!fout.is_open()) {
        return 1;
    }

    auto build_time_start = chrono::high_resolution_clock::now();
    build_all("gtfs-data-newyork2");
    auto build_time_end = chrono::high_resolution_clock::now();

    cout << chrono::duration<double>(build_time_end - build_time_start).count() << endl;

    vector<int> stop_ids;
    stop_ids.reserve(StopCoords.size());
    for (const auto& stop_coords : StopCoords) {
        stop_ids.push_back(stop_coords.first);
    }

    std::random_device rd; 
    std::mt19937 gen(rd()); 
    std::uniform_int_distribution<> distrib(0, stop_ids.size() - 1); // random source/dest stop
    std::uniform_int_distribution<int> dep_dist(36000, 64800); // random departure time between 10AM and 6PM

    auto raptor_time_start = chrono::high_resolution_clock::now();
    for (int iter = 0; iter < iterations; ++iter) {
        int dep_time = dep_dist(gen);
        int K = 5;

        int source_stop = stop_ids[distrib(gen)];
        int dest_stop = stop_ids[distrib(gen)];
        while (dest_stop == source_stop) {
            dest_stop = stop_ids[distrib(gen)];
        }

        auto [arr_time, path] = raptor(source_stop, dest_stop, dep_time, K);

        if (arr_time == -1) {
            fout << "Source stop: " << source_stop << '\n';
            fout << "Dest stop: " << dest_stop << '\n';
            fout << "Departure time: " << seconds_to_time(dep_time) << '\n';
            fout << "No path found.\n";
            fout << "============================================" << '\n';
            fout << '\n';
            continue;
        }

        fout << "Source stop: " << source_stop << '\n';
        fout << "Dest stop: " << dest_stop << '\n';
        fout << "Departure time: " << seconds_to_time(dep_time) << '\n';
        fout << "Arrival time: " << seconds_to_time(arr_time) << '\n';
        fout << "Transfers: " << path.size() - 1 << '\n';
        fout << '\n';

        for (size_t i = 0; i < path.size(); ++i) {
            const auto& transfer = path[i];
            fout << i + 1 << " - ";
            if (transfer.type == "walk") {
                fout << "WALK:" << '\n';
                fout << "Walk from stop " << transfer.stop1
                    << " to stop " << transfer.stop2 << '\n';
                fout << "Start: " << seconds_to_time(transfer.start_time)
                    << ", End: " << seconds_to_time(transfer.end_time) << '\n';
                fout << "Walking time: " << transfer.walk_time / 60
                    << " min " << transfer.walk_time % 60 << " s" << '\n';
            } else {
                fout << "BUS/TRAIN:" << '\n';
                fout << "Board stop " << transfer.stop1
                    << "; Get down at stop " << transfer.stop2 << '\n';
                fout << "Start: " << seconds_to_time(transfer.start_time)
                    << ", End: " << seconds_to_time(transfer.end_time) << '\n';
                int transit_time = transfer.end_time - transfer.start_time;
                fout << "Transit time: " << transit_time / 60
                    << " min " << transit_time % 60 << " s" << '\n';
            }
            fout << '\n';
        }
        fout << "============================================" << '\n';
        fout << '\n';
    }
    auto raptor_time_end = chrono::high_resolution_clock::now();
    cout << chrono::duration<double>(raptor_time_end - raptor_time_start).count() << endl;

    fout.close();
    return 0;
}