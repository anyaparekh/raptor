#include "gtfs.h"

#include <iostream>
#include <fstream>
#include <unordered_set>
#include <cmath>
#include <algorithm>
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

    for (auto &row : reader) {
        StopTimeHeaders entry;

        entry.trip_id = row["trip_id"].get<>();
        entry.arrival_time = row["arrival_time"].get<>();
        entry.departure_time = row["departure_time"].get<>();
        entry.stop_id = row["stop_id"].get<int>();
        entry.stop_sequence = row["stop_sequence"].get<int>();
        entry.stop_headsign = row["stop_headsign"].get<>();
        entry.pickup_type = row["pickup_type"].get<>();
        entry.shape_dist_traveled = row["shape_dist_traveled"].get<>();

        df_stop_times.push_back(entry);
    }
}

void load_trips(const string &path) {
    csv::CSVReader reader(path);

    for (auto &row : reader) {
        TripHeaders entry;
        entry.route_id  = row["route_id"].get<>();
        entry.service_id = row["service_id"].get<>();
        entry.trip_id = row["trip_id"].get<>();
        entry.direction_id = row["direction_id"].get<>();
        entry.block_id = row["block_id"].get<>();
        entry.shape_id = row["shape_id"].get<>();
        entry.direction = row["direction"].get<>();
        entry.wheelchair_accessible = row["wheelchair_accessible"].get<>();
        entry.schd_trip_id = row["schd_trip_id"].get<>();

        df_trips.push_back(entry);
    }
}

void load_routes(const string &path) {
    csv::CSVReader reader(path);

    for (auto &row : reader) {
        RouteHeaders entry;
        entry.route_id = row["route_id"].get<>();
        entry.route_short_name = row["route_short_name"].get<>();
        entry.route_long_name = row["route_long_name"].get<>();
        entry.route_type = row["route_type"].get<>();
        entry.route_url = row["route_url"].get<>();
        entry.route_color = row["route_color"].get<>();
        entry.route_text_color = row["route_text_color"].get<>();

        df_routes.push_back(entry);
    }
}

void load_stops(const string &path) {
    csv::CSVReader reader(path);

    for (auto &row : reader) {
        StopHeaders entry;

        entry.stop_id   = row["stop_id"].get<int>();
        entry.stop_code = row["stop_code"].get<>();
        entry.stop_name = row["stop_name"].get<>();
        entry.stop_desc = row["stop_desc"].get<>();
        entry.stop_lat  = row["stop_lat"].get<double>();
        entry.stop_lon  = row["stop_lon"].get<double>();
        entry.location_type = row["location_type"].get<>();
        entry.parent_station = row["parent_station"].get<>();
        entry.wheelchair_boarding = row["wheelchair_boarding"].get<>();

        df_stops.push_back(entry);
    }
}

static vector<MergedRow> merge_stop_times_trips() {
    unordered_map<string,string> trip_to_route;
    for (auto &t : df_trips)
        trip_to_route[t.trip_id] = t.route_id;

    vector<MergedRow> merged;
    merged.reserve(df_stop_times.size());

    for (auto &st : df_stop_times) {
        MergedRow entry;
        entry.trip_id = st.trip_id;
        entry.route_id = trip_to_route[st.trip_id];
        entry.stop_id = st.stop_id;
        entry.stop_sequence = st.stop_sequence;
        entry.arrival_time = st.arrival_time;
        entry.departure_time = st.departure_time;
        merged.push_back(entry);
    }
    return merged;
}

void build_route_trips() {
    vector<MergedRow> merged = merge_stop_times_trips();

    sort(merged.begin(), merged.end(),
         [](auto &a, auto &b){
            if (a.route_id != b.route_id) return a.route_id < b.route_id;
            if (a.trip_id != b.trip_id) return a.trip_id < b.trip_id;
            return a.stop_sequence < b.stop_sequence;
         });

    for (auto &mr : merged) {
        auto &vec = RouteStops[mr.route_id];
        if (find(vec.begin(), vec.end(), mr.stop_id) == vec.end())
            vec.push_back(mr.stop_id);
    }

    for (auto &t : df_trips)
        RouteTrips[t.route_id].push_back(t.trip_id);

    for (auto &mr : merged)
        StopRoutes[mr.stop_id].insert(mr.route_id);
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
    
    sort(df_stop_times.begin(), df_stop_times.end(),
         [](auto &a, auto &b){
            if (a.trip_id != b.trip_id) return a.trip_id < b.trip_id;
            return a.stop_sequence < b.stop_sequence;
         });

    string cur_trip = "";
    unordered_map<int,pair<int,int>> stops_dict;

    for (auto &row : df_stop_times) {
        if (row.trip_id != cur_trip && cur_trip != "") {
            Trips[cur_trip].stops = std::move(stops_dict);
            stops_dict.clear();
        }
        cur_trip = row.trip_id;
        stops_dict[row.stop_id] = {
            gtfs_time_to_seconds(row.arrival_time),
            gtfs_time_to_seconds(row.departure_time)
        };
    }
    if (cur_trip != "")
        Trips[cur_trip].stops = std::move(stops_dict);
}

void build_transfers() {
    for (auto &s : df_stops)
        StopCoords[s.stop_id] = {s.stop_lat, s.stop_lon};

    stop_ids.clear();
    for (auto &p : StopCoords)
        stop_ids.push_back(p.first);

    sort(stop_ids.begin(), stop_ids.end());

    for (size_t i = 0; i < stop_ids.size(); ++i) {
        int s1 = stop_ids[i];
        auto &c1 = StopCoords[s1];

        for (size_t j = i + 1; j < stop_ids.size(); ++j) {
            int s2 = stop_ids[j];
            auto &c2 = StopCoords[s2];

            double dist = get_walking_distance(c1.first, c1.second, c2.first, c2.second);

            if (dist <= 1500.0) {
                int walk = int(dist / 1.4);
                Transfers[s1].push_back({s2, walk});
                Transfers[s2].push_back({s1, walk});
            }
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
