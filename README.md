# Mini Roaring Bitmap — optimized C++20

Drop-in replacement for the earlier learning project. No external dependencies.

## Files
- `array_container.hpp`: sparse sorted integers
- `bitmap_container.hpp`: bitmap words and range operations
- `run_container.hpp`: consecutive ranges and iteration
- `roaring.hpp`: 32-bit Roaring wrapper, deep-copyable indirect bitmap storage, AND/OR, run optimization
- `main.cpp`: randomized correctness and boundary tests
- `benchmark.cpp`: benchmark with *estimated* storage accounting

## MSVC 2026 (Developer Command Prompt)

```cmd
cl /std:c++20 /EHsc /W4 /Od main.cpp /Fe:tests.exe
tests.exe
cl /std:c++20 /EHsc /O2 /W4 benchmark.cpp /Fe:benchmark.exe
benchmark.exe
```

Build tests and benchmarks separately (each has `main()`).

### Memory measurement caveat
`estimated_memory_usage()` counts owned buffers and estimates `std::map` node overhead. `std::set` and `std::unordered_set` baselines also estimate allocator overhead. Numbers are not exact live allocations or process RSS, and are not directly equivalent to each other. Benchmark operations are illustrative rather than a statistically rigorous matched-workload study.

### Current limitations
- `std::map` incurs overhead for wide-random 32-bit IDs and comparisons are logarithmic.
- Some mixed-type set operations enumerate values; Run–Bitmap operations instead combine word-level bitmaps.
- `run_optimize()` evaluates run compression only when explicitly invoked and can be relatively slow on random data.
- Single-threaded; not a serialized production Roaring format.
