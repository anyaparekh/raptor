#include <iostream>
#include <zip.h>
#include <fstream>
#include <sys/stat.h>
#include <filesystem>
#include <random>
#include <vector>
#include <chrono>
#include <cassert>
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

size_t count_csv_rows(const string &path) {
    ifstream in(path);
    string line;
    size_t rows = 0;
    while (std::getline(in, line)) {
        if (!line.empty()) {
            rows++;
        }
    }
    return rows-1;
}

pair<unordered_set<string>,int> expected_earliest_trip(const string &route_id, int board_stop, int board_time) {
    const auto &trips = RouteTrips[route_id];
    string best_trip = "";
    int best_dep_time = numeric_limits<int>::max();
    unordered_set<string> best_trips;

    for (size_t i = 0; i < trips.size(); ++i) {
        const auto &trip_id = trips[i];
        const auto &stops = Trips[trip_id].stops;
        if (stops.find(board_stop) == stops.end()) {
            continue;
        }
        int dep_time = stops.at(board_stop).second;
        if (dep_time >= board_time && dep_time < best_dep_time) {
            best_dep_time = dep_time;
            best_trips.clear();
            best_trips.insert(trip_id);
        } else if (dep_time == best_dep_time) {
            best_trips.insert(trip_id);
        }
    }
    if (best_trips.empty()) {
        return {best_trips, -1};
    }
    return {best_trips, best_dep_time};
}

void conduct_unit_tests(string dataset) {
    size_t stops_file_rows = count_csv_rows(dataset + "/stops.txt");
    size_t trips_file_rows = count_csv_rows(dataset + "/trips.txt");
    size_t routes_file_rows = count_csv_rows(dataset + "/routes.txt");

    assert(stops_file_rows == df_stops.size());
    assert(stops_file_rows == StopCoords.size());
    assert(stops_file_rows == Transfers.size());
    assert(trips_file_rows == df_trips.size());
    assert(routes_file_rows == df_routes.size());

    cout << "Assert passed - CSV row counts match data structures." << endl;

    vector<int> stop_ids;
    stop_ids.reserve(StopRoutes.size());
    for (const auto &stop : StopRoutes) {
        stop_ids.push_back(stop.first);
    }

    mt19937 gen(1);
    uniform_int_distribution<size_t> dist(0, stop_ids.size()-1);

    unordered_set<int> rand_stops;
    for (int i = 0; i < 5; ++i) {
        rand_stops.insert(stop_ids[dist(gen)]);
    }

    for (int stop_id : rand_stops) {
        const auto &routes = StopRoutes[stop_id];
        assert(!routes.empty());

        for (const auto &route_id : routes) {
            assert(RouteStops.find(route_id) != RouteStops.end());
            const auto &route_stop_list = RouteStops[route_id];
            assert(find(route_stop_list.begin(), route_stop_list.end(), stop_id) != route_stop_list.end());
        }
    }
    cout << "Assert passed - StopRoutes entries validated for 5 random stops\n";
    
    vector<string> routes;
    for (const auto& route : RouteStops) {
        if (!route.second.empty()) {
            routes.push_back(route.first);
        }
    }

    uniform_int_distribution<size_t> dist2(0, routes.size()-1);
    string test_route = routes[dist2(gen)];
    assert(RouteStops.find(test_route) != RouteStops.end());

    int board_stop = RouteStops[test_route].front();
    int board_time = 0;

    auto expected = expected_earliest_trip(test_route, board_stop, board_time);
    string found_trip = earliest_trip(test_route, board_stop, board_time);

    assert(expected.first.find(found_trip) != expected.first.end());
    cout << "Assert passed - earliest_trip returned expected trip id for route " << test_route << '\n';

    cout << "ALL ASSERTIONS PASSED\n";
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
    string dataset = "gtfs-data-newyork2";

    auto build_time_start = chrono::high_resolution_clock::now();
    build_all(dataset);
    auto build_time_end = chrono::high_resolution_clock::now();

    cout << chrono::duration<double>(build_time_end - build_time_start).count() << endl;

    conduct_unit_tests(dataset);

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