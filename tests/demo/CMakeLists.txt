cmake_minimum_required(VERSION 3.25)

# ----------------------------------------------------------------------------
# Demo executables for loki_core statistical modules:
#   demo_sampling    -- Sampler: distribution histograms
#   demo_bootstrap   -- Bootstrap CI (percentile, BCa, block)
#   demo_permutation -- Permutation tests (one-sample, two-sample, correlation)
#
# All demos depend on loki::core only.
# Input data: tests/demo/input/CLIM_DATA_EX1.txt
# Output:     tests/demo/png/      (gnuplot PNG plots)
#             tests/demo/protocol/ (text protocols)
# ----------------------------------------------------------------------------

add_executable(demo_sampling
    demo_sampling.cpp
)
target_link_libraries(demo_sampling
    PRIVATE loki::core
    PRIVATE loki_compiler_options
)

add_executable(demo_bootstrap
    demo_bootstrap.cpp
)
target_link_libraries(demo_bootstrap
    PRIVATE loki::core
    PRIVATE loki_compiler_options
)

add_executable(demo_permutation
    demo_permutation.cpp
)
target_link_libraries(demo_permutation
    PRIVATE loki::core
    PRIVATE loki_compiler_options
)
