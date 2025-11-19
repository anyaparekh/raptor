# RAPTOR Reimplementation

The main branch contains all optimizations. To run each of the optimizations, checkout to its branch!
## Optimization Branches

| Branch | Description |
|:-------|:-------------|
| **baseline** | The original RAPTOR algorithm, as specified in the [research paper](https://www.microsoft.com/en-us/research/wp-content/uploads/2012/01/raptor_alenex.pdf). |
| **opt-1** | Optimizing GTFS preprocessing |
| **opt-2** | Parallelizing earliest trip selection |
| **opt-3** | Parallelizing Q-Route scanning |
| **opt-4** | Changing vector-based membership to set-based membership |


## Command Line Instructions
Run the following commands to build and run the full RAPTOR algorithm (check out the notes section for information on custom parameters and the unit-test suite).

```
make clean
make -j
./main.exe <num_iters> --dataset <dataset_name> --source <source_stop_id> --dest <dest_stop_id> --departure <departure_time>`
```

#### Notes:
* <num_iters>: Defaults to 500 iterations
* <dataset_name>: Defaults to _gtfs-data_, which represents the Chicago GTFS data. Alternative is the _gtfs-data-newyork2_ dataset, which represents the New York GTFS data
* <source_stop_id>: Defaults to random source stop, specify source stop id from the respective id in the `stops.txt` file
* <dest_stop_id>: Defaults to random destination stop, specify destination stop id from the respective id in the `stops.txt` file
* <departure_time>: Defaults to random time between 10AM-6PM, specify time based on seconds past midnight
