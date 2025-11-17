#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <limits>
#include <algorithm>
#include <tuple>
#include "gtfs.h"

using namespace std;

struct PathStep {
    string type;
    int stop1;
    int stop2;
    string trip_id;
    int walk_time;
    int start_time;
    int end_time;
    int round;
};

string earliest_trip(const string& route_id, int board_stop, int board_time);

pair<int, vector<PathStep>> raptor(int source_stop, int dest_stop, int departure_time, int K);
