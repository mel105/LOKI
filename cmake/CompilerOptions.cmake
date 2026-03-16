# cmake/CompilerOptions.cmake
# ─────────────────────────────────────────────────────────────────────────────
# Centralised compiler flags for all LOKI targets.
# Usage: call loki_set_compiler_options(<target>) on every library/app target.
# ─────────────────────────────────────────────────────────────────────────────

option(LOKI_ENABLE_SANITIZERS "Enable AddressSanitizer + UndefinedBehaviourSanitizer" OFF)

# Interface library that carries flags — targets link against it privately.
add_library(loki_compiler_options INTERFACE)

target_compile_options(loki_compiler_options INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Werror           # treat warnings as errors
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wconversion
    -Wsign-conversion
    -Wmisleading-indentation
    -Wduplicated-cond
    -Wduplicated-branches
    -Wlogical-op
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
)

if(LOKI_ENABLE_SANITIZERS)
    target_compile_options(loki_compiler_options INTERFACE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(loki_compiler_options INTERFACE
        -fsanitize=address,undefined
    )
endif()

# Convenience function — call once per target instead of repeating link lines.
function(loki_set_compiler_options target)
    target_link_libraries(${target} PRIVATE loki_compiler_options)
endfunction()
