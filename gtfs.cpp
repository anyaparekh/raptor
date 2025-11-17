#include "gtfs.h"

#include <iostream>
#include <fstream>
#include <unordered_set>
#include <cmath>
#include <algorithm>
#include <omp.h>
#include "csv.hpp"

using namespace std;

vector<StopTimeHeaders> df_stop_times;
vector<TripHeaders> df_trips;
vector<RouteHeaders> df_routes;
vector<StopHeaders> df_stops;

unordered_map<string, vector<int>> RouteStops;
unordered_map<string, vector<string>> RouteTrips;
unordered_map<string, TripInfo> Trips;
unordered_map<int, unordered_set<string>> StopRoutes;
unordered_map<int, pair<double,double>> StopCoords;
unordered_map<int, vector<pair<int,int>>> Transfers;
vector<int> stop_ids;

int gtfs_time_to_seconds(const string &time_str) {
    int hours = 0, mins = 0, secs = 0;
    sscanf(time_str.c_str(), "%d:%d:%d", &hours, &mins, &secs);
    return hours*3600 + mins*60 + secs;
}

static double to_rads(double d) { 
    return d * M_PI / 180.0; 
}

double get_walking_distance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000.0;
    double dLat = to_rads(lat2 - lat1);
    double dLon = to_rads(lon2 - lon1);

    double a = sin(dLat/2)*sin(dLat/2) + cos(to_rads(lat1))*cos(to_rads(lat2)) * sin(dLon/2)*sin(dLon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    return R * c;
}

void load_stop_times(const string &path) {
    csv::CSVReader reader(path);
    vector<csv::CSVRow> rows;
    for (auto& row : reader) {
        rows.push_back(row);
    }
    df_stop_times.resize(rows.size());

    #pragma omp parallel for num_threads(4)
    for (int i = 0; i < rows.size(); ++i) {
        StopTimeHeaders entry;

        entry.trip_id = rows[i]["trip_id"].get<>();
        entry.arrival_time = rows[i]["arrival_time"].get<>();
        entry.departure_time = rows[i]["departure_time"].get<>();
        entry.stop_id = rows[i]["stop_id"].get<int>();
        entry.stop_sequence = rows[i]["stop_sequence"].get<int>();
        entry.stop_headsign = rows[i]["stop_headsign"].get<>();
        entry.pickup_type = rows[i]["pickup_type"].get<>();
        entry.shape_dist_traveled = rows[i]["shape_dist_traveled"].get<>();

        df_stop_times[i] = move(entry);
    }
}

void load_trips(const string &path) {
    csv::CSVReader reader(path);
    vector<csv::CSVRow> rows;
    for (auto& row : reader) {
        rows.push_back(row);
    }
    df_trips.resize(rows.size());

    #pragma omp parallel for num_threads(4)
    for (size_t i = 0; i < rows.size(); ++i) {
        TripHeaders entry;
        entry.route_id  = rows[i]["route_id"].get<>();
        entry.service_id = rows[i]["service_id"].get<>();
        entry.trip_id = rows[i]["trip_id"].get<>();
        entry.direction_id = rows[i]["direction_id"].get<>();
        entry.block_id = rows[i]["block_id"].get<>();
        entry.shape_id = rows[i]["shape_id"].get<>();
        entry.direction = rows[i]["direction"].get<>();
        entry.wheelchair_accessible = rows[i]["wheelchair_accessible"].get<>();
        entry.schd_trip_id = rows[i]["schd_trip_id"].get<>();

        df_trips[i] = move(entry);
    }
}

void load_routes(const string &path) {
    csv::CSVReader reader(path);
    vector<csv::CSVRow> rows;
    for (auto& row : reader) {
        rows.push_back(row);
    }
    df_routes.resize(rows.size());

    #pragma omp parallel for num_threads(4)
    for (size_t i = 0; i < rows.size(); ++i) {
        RouteHeaders entry;
        entry.route_id = rows[i]["route_id"].get<>();
        entry.route_short_name = rows[i]["route_short_name"].get<>();
        entry.route_long_name = rows[i]["route_long_name"].get<>();
        entry.route_type = rows[i]["route_type"].get<>();
        entry.route_url = rows[i]["route_url"].get<>();
        entry.route_color = rows[i]["route_color"].get<>();
        entry.route_text_color = rows[i]["route_text_color"].get<>();

        df_routes[i] = move(entry);
    }
}

void load_stops(const string &path) {
    csv::CSVReader reader(path);
    vector<csv::CSVRow> rows;
    for (auto& row : reader) {
        rows.push_back(row);
    }
    df_stops.resize(rows.size());

    #pragma omp parallel for num_threads(4)
    for (size_t i = 0; i < rows.size(); ++i) {
        StopHeaders entry;

        entry.stop_id = rows[i]["stop_id"].get<int>();
        entry.stop_code = rows[i]["stop_code"].get<>();
        entry.stop_name = rows[i]["stop_name"].get<>();
        entry.stop_desc = rows[i]["stop_desc"].get<>();
        entry.stop_lat = rows[i]["stop_lat"].get<double>();
        entry.stop_lon = rows[i]["stop_lon"].get<double>();
        entry.location_type = rows[i]["location_type"].get<>();
        entry.parent_station = rows[i]["parent_station"].get<>();
        entry.wheelchair_boarding = rows[i]["wheelchair_boarding"].get<>();

        df_stops[i] = move(entry);
    }
}

static vector<MergedRow> merge_stop_times_trips() {
    unordered_map<string,string> trip_to_route;
    for (auto &t : df_trips)
        trip_to_route[t.trip_id] = t.route_id;

    vector<MergedRow> merged(df_stop_times.size());

    #pragma omp parallel for num_threads(4)
    for (size_t i = 0; i < df_stop_times.size(); ++i) {
        auto &st = df_stop_times[i];
        MergedRow entry;
        entry.trip_id = st.trip_id;
        entry.route_id = trip_to_route[st.trip_id];
        entry.stop_id = st.stop_id;
        entry.stop_sequence = st.stop_sequence;
        entry.arrival_time = st.arrival_time;
        entry.departure_time = st.departure_time;
        merged[i] = move(entry);
    }
    return merged;
}

void build_route_trips() {
    vector<MergedRow> merged = merge_stop_times_trips();

    unordered_map<string, unordered_set<int>> route_stops_seen;
    for (auto &row : merged) {
        if (route_stops_seen[row.route_id].insert(row.stop_id).second) {
            RouteStops[row.route_id].push_back(row.stop_id);
        }
        StopRoutes[row.stop_id].insert(row.route_id);
    }

    for (auto &t : df_trips)
        RouteTrips[t.route_id].push_back(t.trip_id);
}

void build_trips() {
    for (auto &t : df_trips) {
        TripInfo entry;
        entry.info["route_id"] = t.route_id;
        entry.info["service_id"] = t.service_id;
        entry.info["direction_id"] = t.direction_id;
        entry.info["block_id"] = t.block_id;
        entry.info["shape_id"] = t.shape_id;
        entry.info["direction"] = t.direction;
        entry.info["wheelchair_accessible"] = t.wheelchair_accessible;
        entry.info["schd_trip_id"] = t.schd_trip_id;
        Trips[t.trip_id] = entry;
    }
    
    // create an map: trip_id -> stop_time indices (tldr group by trip_id) 
    unordered_map<string, vector<size_t>> trip_to_indices;
    trip_to_indices.reserve(Trips.size());
    
    for (size_t i = 0; i < df_stop_times.size(); ++i) {
        trip_to_indices[df_stop_times[i].trip_id].push_back(i);
    }
    
    // can't parallelize unordered_map, so converting to vector of pairs instead
    vector<pair<string, vector<size_t>>> trip_groups;
    for (auto &kv : trip_to_indices) {
        trip_groups.push_back(pair<string, vector<size_t>>(move(kv.first), move(kv.second)));
    }
    
    #pragma omp parallel for num_threads(4)
    for (size_t i = 0; i < trip_groups.size(); ++i) {
        auto &[trip_id, indices] = trip_groups[i];
        
        unordered_map<int, pair<int,int>> stops_dict;
        stops_dict.reserve(indices.size());
        
        for (size_t idx : indices) {
            auto &row = df_stop_times[idx];
            stops_dict[row.stop_id] = {
                gtfs_time_to_seconds(row.arrival_time),
                gtfs_time_to_seconds(row.departure_time)
            };
        }
        
        Trips[trip_id].stops = move(stops_dict);
    }
}

void build_transfers() {
    for (auto &s : df_stops)
        StopCoords[s.stop_id] = {s.stop_lat, s.stop_lon};

    stop_ids.clear();
    for (auto &p : StopCoords)
        stop_ids.push_back(p.first);

    vector<unordered_map<int, vector<pair<int,int>>>> thread_maps(4);
    #pragma omp parallel num_threads(4)
    {
        int tid = omp_get_thread_num();
        auto &local_map = thread_maps[tid];

        #pragma omp for schedule(dynamic)
        for (size_t i = 0; i < stop_ids.size(); ++i) {
            int s1 = stop_ids[i];
            auto &c1 = StopCoords[s1];

            for (size_t j = i + 1; j < stop_ids.size(); ++j) {
                int s2 = stop_ids[j];
                auto &c2 = StopCoords[s2];

                double dist = get_walking_distance(c1.first, c1.second, c2.first, c2.second);

                if (dist <= 1500.0) {
                    int walk = int(dist / 1.4);
                    local_map[s1].push_back({s2, walk});
                    local_map[s2].push_back({s1, walk});
                }
            }
        }
    }
    for (auto &map : thread_maps) {
        for (auto &p : map) {
            auto &transfer = Transfers[p.first];
            transfer.insert(transfer.end(), p.second.begin(), p.second.end());
        }
    }
}

void build_all(const string &base_dir) {

    load_stop_times(base_dir + "/stop_times.txt");
    load_trips(base_dir + "/trips.txt");
    load_routes(base_dir + "/routes.txt");
    load_stops(base_dir + "/stops.txt");

    build_route_trips();
    build_trips();
    build_transfers();
}
