# Benchmark programs

# Latency benchmark
add_executable(benchmark_latency benchmark_latency.c)
target_include_directories(benchmark_latency PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(TARGET midix_static)
    target_link_libraries(benchmark_latency PRIVATE midix_static)
else()
    target_link_libraries(benchmark_latency PRIVATE midix_shared)
endif()

# Additional frameworks for latency benchmark (CoreMIDI)
if(APPLE)
    target_link_libraries(benchmark_latency PRIVATE
        "-framework CoreMIDI"
        "-framework CoreFoundation"
    )
endif()

# Throughput benchmark
add_executable(benchmark_throughput benchmark_throughput.c)
target_include_directories(benchmark_throughput PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(TARGET midix_static)
    target_link_libraries(benchmark_throughput PRIVATE midix_static)
else()
    target_link_libraries(benchmark_throughput PRIVATE midix_shared)
endif()

# Custom target to run all benchmarks
add_custom_target(run_benchmarks
    COMMAND echo "=== Running Latency Benchmark ==="
    COMMAND ./benchmark_latency > ../benchmark_results_latency.txt 2>&1 || true
    COMMAND echo ""
    COMMAND echo "=== Running Throughput Benchmark ==="
    COMMAND ./benchmark_throughput > ../benchmark_results_throughput.txt 2>&1 || true
    COMMAND echo ""
    COMMAND echo "Results saved to:"
    COMMAND echo "  - benchmark_results_latency.txt"
    COMMAND echo "  - benchmark_results_throughput.txt"
    DEPENDS benchmark_latency benchmark_throughput
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running all benchmarks..."
)
