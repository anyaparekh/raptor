## Command Line Instructions
Run the following commands and add custom parameters to run the RAPTOR algorithm

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
