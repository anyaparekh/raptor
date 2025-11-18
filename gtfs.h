#ifndef GTFS_H
#define GTFS_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>

struct StopTimeHeaders {
    std::string trip_id;
    std::string arrival_time;
    std::string departure_time;
    int stop_id;
};

struct TripHeaders {
    std::string route_id;
    std::string trip_id;
};

struct RouteHeaders {
    std::string route_id;
};

struct StopHeaders {
    int stop_id;
    double stop_lat;
    double stop_lon;
};

struct TripInfo {
    std::unordered_map<std::string, std::string> info; // trip metadata info
    std::unordered_map<int, std::pair<int,int>> stops;
};

struct MergedRow {
    std::string trip_id;
    std::string route_id;
    int stop_id;
    std::string arrival_time;
    std::string departure_time;
};

extern std::vector<StopTimeHeaders> df_stop_times;
extern std::vector<TripHeaders> df_trips;
extern std::vector<RouteHeaders> df_routes;
extern std::vector<StopHeaders> df_stops;

extern std::unordered_map<std::string, std::vector<int>> RouteStops;

extern std::unordered_map<std::string, std::vector<std::string>> RouteTrips;

// Trips - {trip_id: {stops: {stop_id: arrival_time, departure_time}}}
extern std::unordered_map<std::string, TripInfo> Trips;

// StopRoutes - {stop_id : [routes]}
extern std::unordered_map<int, std::unordered_set<std::string>> StopRoutes;

// StopCoords - {stop_id: (stop_lat, stop_lon)}
extern std::unordered_map<int, std::pair<double,double>> StopCoords;

// Transfers - {stop: [(transfer_stop, walk_time)]}
extern std::unordered_map<int, std::vector<std::pair<int,int>>> Transfers;

extern std::vector<int> stop_ids;

int gtfs_time_to_seconds(const std::string &time_str);
double get_walking_distance(double lat1, double lon1, double lat2, double lon2);

void load_stop_times(const std::string &path);
void load_trips(const std::string &path);
void load_routes(const std::string &path);
void load_stops(const std::string &path);

void build_all(const std::string &base_dir);

#endif
