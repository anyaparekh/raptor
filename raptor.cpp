#include "raptor.h"
#include <iostream>
#include <map>
#include <set>
#include <cstdio>
#include <omp.h>

using namespace std;

struct TakenStep {
    int prev_stop;
    int prev_round;
    string mode;
    int walk_time;
};

string earliest_trip(const string& route_id, int board_stop, int board_time) {
    string best_trip = "";
    int best_dep = numeric_limits<int>::max();

    omp_set_num_threads(4);

    #pragma omp parallel
    {
        string local_best_trip = "";
        int local_best_dep = numeric_limits<int>::max();

        #pragma omp for schedule(dynamic) nowait
        for (size_t i = 0; i < RouteTrips[route_id].size(); i++) {
            const string &trip_id = RouteTrips[route_id][i];
            const auto &stops = Trips[trip_id].stops;
            if (stops.count(board_stop) == 0) {
                continue;
            }
            int dep = stops.at(board_stop).second;
            if (dep >= board_time && dep < local_best_dep) {
                local_best_dep = dep;
                local_best_trip = trip_id;
            }
        }

        #pragma omp critical
        {
            if (!local_best_trip.empty() && local_best_dep < best_dep) {
                best_dep = local_best_dep;
                best_trip = local_best_trip;
            }
        }
    }
    return best_trip;
}


pair<int, vector<PathStep>> raptor(int source_stop, int dest_stop, int departure_time, int K) {
    unordered_map<int, vector<int>> stop_arrival_times;
    unordered_map<int, int> earliest_stop_arrival_times;

    for (const auto& kv : StopCoords) {
        int stop = kv.first;
        stop_arrival_times[stop] = vector<int>(K + 1, numeric_limits<int>::max());
        earliest_stop_arrival_times[stop] = numeric_limits<int>::max();
    }

    stop_arrival_times[source_stop][0] = departure_time;
    earliest_stop_arrival_times[source_stop] = departure_time;
    map<pair<int,int>, TakenStep> route_taken;

    vector<int> marked_stops = { source_stop };

    omp_set_num_threads(4);

    for (int k = 1; k < K+1; ++k) {
        unordered_map<string,int> Q;

        if (marked_stops.size() <= 200) {
            for (int marked_stop : marked_stops) {
                for (const string& route_id : StopRoutes[marked_stop]) {
                    const auto& stops = RouteStops[route_id];
                    auto marked_stop_it = std::find(stops.begin(), stops.end(), marked_stop);

                    if (marked_stop_it == stops.end())
                        continue;

                    int marked_stop_idx = static_cast<int>(marked_stop_it - stops.begin());

                    if (!Q.count(route_id) || marked_stop_idx < Q[route_id]) {
                        Q[route_id] = marked_stop_idx;
                    }
                }
            }
        } else {
            #pragma omp parallel
            {
                unordered_map<string,int> local_Q;

                #pragma omp for schedule(dynamic)
                for (int i = 0; i < (int)marked_stops.size(); i++) {
                    int marked_stop = marked_stops[i];

                    for (const string& route_id : StopRoutes[marked_stop]) {
                        const auto& stops = RouteStops[route_id];
                        auto marked_stop_it = std::find(stops.begin(), stops.end(), marked_stop);

                        if (marked_stop_it == stops.end())
                            continue;

                        int marked_stop_idx = static_cast<int>(marked_stop_it - stops.begin());

                        if (!local_Q.count(route_id) || marked_stop_idx < local_Q[route_id]) {
                            local_Q[route_id] = marked_stop_idx;
                        }
                    }
                }

                #pragma omp critical
                {
                    for (const auto& local_Q_routestop : local_Q) {
                        const string& route_id = local_Q_routestop.first;
                        int marked_stop_idx = local_Q_routestop.second;

                        if (!Q.count(route_id) || marked_stop_idx < Q[route_id]) {
                            Q[route_id] = marked_stop_idx;
                        }
                    }
                }
            }
        }

        marked_stops.clear();

        for (const auto &route_stop : Q) {
            const string &route_id = route_stop.first;
            int stop_id = route_stop.second;

            if (!RouteStops.count(route_id)) continue;
            const auto &route_stops = RouteStops[route_id];

            int boarding_stop = route_stops[stop_id];
            int boarding_time = stop_arrival_times[boarding_stop][k - 1];
            if (boarding_time == numeric_limits<int>::max()) 
                continue;

            string current_trip = earliest_trip(route_id, boarding_stop, boarding_time);
            if (current_trip.empty()) continue;

            const auto &trip_stops = Trips[current_trip].stops;
            int curr_trip_dep_time = trip_stops.at(boarding_stop).second;

            for (int idx = stop_id; idx < (int)route_stops.size(); idx++) {
                int next_stop = route_stops[idx];
                if (!trip_stops.count(next_stop)) continue;

                int curr_trip_arr_time = trip_stops.at(next_stop).first;
                if (curr_trip_arr_time < curr_trip_dep_time) continue;

                if (curr_trip_arr_time < stop_arrival_times[next_stop][k]) {
                    stop_arrival_times[next_stop][k] = curr_trip_arr_time;
                    earliest_stop_arrival_times[next_stop] = min(earliest_stop_arrival_times[next_stop], curr_trip_arr_time);

                    route_taken[{next_stop, k}] = { boarding_stop, k - 1, current_trip, 0 };

                    if (find(marked_stops.begin(), marked_stops.end(), next_stop) == marked_stops.end()) {
                        marked_stops.push_back(next_stop);
                    }
                }
            }
        }

        vector<int> marked_stops_temp;
        for (int stop : marked_stops) {
            if (Transfers.find(stop) == Transfers.end()) {
                continue;
            }

            int base_prev_time = stop_arrival_times[stop][k - 1];
            if (base_prev_time == numeric_limits<int>::max()) {
                continue;
            }

            for (const auto& w : Transfers[stop]) {
                int walkable_stop = w.first;
                int walk_time = w.second;
                int curr_walk_arr_time = base_prev_time + walk_time;

                if (curr_walk_arr_time < stop_arrival_times[walkable_stop][k]) {
                    stop_arrival_times[walkable_stop][k] = curr_walk_arr_time;
                    earliest_stop_arrival_times[walkable_stop] = min(earliest_stop_arrival_times[walkable_stop], curr_walk_arr_time);

                    route_taken[{walkable_stop, k}] = { stop, k - 1, "walk", walk_time };

                    if (find(marked_stops_temp.begin(), marked_stops_temp.end(), walkable_stop) == marked_stops_temp.end()) {
                        marked_stops_temp.push_back(walkable_stop);
                    }
                }
            }
        }

        marked_stops.insert(marked_stops.end(), marked_stops_temp.begin(), marked_stops_temp.end());

        if (marked_stops.empty()) {
            break;
        }
    }

    int best_time = earliest_stop_arrival_times[dest_stop];

    if (best_time == numeric_limits<int>::max()) {
        return { -1, {} };
    }

    vector<int> round_times_for_dest_stop = stop_arrival_times[dest_stop];

    int rounds_taken = -1;
    for (int k = 0; k < K + 1; ++k) {
        if (round_times_for_dest_stop[k] == best_time) {
            rounds_taken = k;
            break;
        }
    }

    vector<PathStep> path;
    int curr_stop = dest_stop;
    int curr_round = rounds_taken;

    while (route_taken.find({curr_stop, curr_round}) != route_taken.end()) {
        const TakenStep& taken_step = route_taken[{curr_stop, curr_round}];

        int prev_stop = taken_step.prev_stop;
        int prev_round = taken_step.prev_round;
        const string& mode = taken_step.mode;
        int walk_time = taken_step.walk_time;

        PathStep step;
        if (mode == "walk") {
            step.type = "walk";
            step.stop1 = prev_stop;
            step.stop2 = curr_stop;
            step.trip_id = "";
            step.walk_time = walk_time;
            step.start_time = stop_arrival_times[prev_stop][prev_round];
            step.end_time = stop_arrival_times[curr_stop][curr_round];
            step.round = curr_round;
        } else {
            step.type = "bus/train";
            step.stop1 = prev_stop;
            step.stop2 = curr_stop;
            step.trip_id = mode;
            step.walk_time = 0;

            TripInfo& ti = Trips[mode];
            const auto& stops_map = ti.stops;

            if (stops_map.count(prev_stop) && stops_map.count(curr_stop)) {
                step.start_time = stops_map.at(prev_stop).second;
                step.end_time = stops_map.at(curr_stop).first;
            } else {
                step.start_time = 0;
                step.end_time = 0;
            }

            step.round = curr_round;
        }
        path.push_back(step);

        curr_stop  = prev_stop;
        curr_round = prev_round;
    }

    reverse(path.begin(), path.end());
    return { best_time, path };
}
