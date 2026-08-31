# Experiment notes

The numbers in the README were produced with:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
SECONDS_RUN=8 ./scripts/run_experiments.sh
```

on Linux 6.x, 8 hardware threads, no other heavy load, `perf_event_paranoid=1`. Re-run on your machine; **ratios** (IPC, cache miss rate, throughput) matter more than absolute cycle counts.

Docker:

```bash
docker compose run --rm --privileged lpt ./scripts/run_experiments.sh
```

`--privileged` (or at least `perf_event_open` + `/proc` + `/sys`) is required for `perf`.
