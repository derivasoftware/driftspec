# Argos gtest+llvm-cov coverage helper (REQ-00469-1).
#
# Ships with the argos-test-gtest plugin and is copied into a project's
# cmake/ directory at `argos init` time when argos-test-gtest is one of
# the installed plugins. The host substrate does not own this file;
# a project that picks a different test framework (catch2, doctest,
# custom main) gets a different helper from a different plugin.
#
# Usage:
#   cmake -S . -B build -DARGOS_COVERAGE=ON
#   cmake --build build
#   ctest --test-dir build
#   cmake --build build --target coverage_report
#
# Requires clang and llvm-cov / llvm-profdata in PATH. The merged
# profdata lands in build/coverage/argos-coverage.profdata; the text
# report is written to stdout and saved to build/coverage/report.txt.
#
# The target binaries are collected from `gtest_discover_tests` —
# every executable that gtest's CMake integration registers — so
# header-only and INTERFACE libraries work out of the box (the bug
# the first martyr pass surfaced: a host-shipped helper assumed
# `${PROJECT_NAME}` was a buildable target).

option(ARGOS_COVERAGE "Enable clang source-based coverage" OFF)

if(ARGOS_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(WARNING
            "ARGOS_COVERAGE expects clang; current compiler is "
            "${CMAKE_CXX_COMPILER_ID}. Flags still applied but the "
            "llvm-cov report target may not work as advertised."
        )
    endif()
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping -O0 -g)
    add_link_options(-fprofile-instr-generate -fcoverage-mapping)

    find_program(LLVM_PROFDATA NAMES llvm-profdata llvm-profdata-18 llvm-profdata-17)
    find_program(LLVM_COV NAMES llvm-cov llvm-cov-18 llvm-cov-17)

    set(_coverage_dir ${CMAKE_BINARY_DIR}/coverage)
    file(MAKE_DIRECTORY ${_coverage_dir})

    # Pin the .profraw drop location for every test ctest runs (the
    # default is whatever cwd ctest invoked the binary in — fragile,
    # especially under multi-config generators). %p expands to the
    # pid so parallel ctest -j N does not overwrite a sibling's file.
    # CMAKE_TEST_LAUNCHER (CMake 3.29+) wraps every test command;
    # ``cmake -E env`` injects the env var for the wrapped process.
    set(CMAKE_TEST_LAUNCHER ${CMAKE_COMMAND} -E env
        LLVM_PROFILE_FILE=${_coverage_dir}/profile-%p.profraw)

    # Defer the coverage_report target wiring until every other
    # CMakeLists.txt declaration has run — only then do all the
    # gtest-registered EXECUTABLE targets exist for collection.
    cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
        CALL _argos_coverage_finalise)

    function(_argos_coverage_finalise)
        set(_objs "")
        function(_argos_collect dir)
            get_property(_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
            foreach(_tgt IN LISTS _targets)
                get_target_property(_type ${_tgt} TYPE)
                if(_type STREQUAL "EXECUTABLE")
                    list(APPEND _objs -object $<TARGET_FILE:${_tgt}>)
                    set(_objs "${_objs}" PARENT_SCOPE)
                endif()
            endforeach()
            get_property(_subs DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
            foreach(_sub IN LISTS _subs)
                _argos_collect(${_sub})
                set(_objs "${_objs}" PARENT_SCOPE)
            endforeach()
        endfunction()
        _argos_collect(${CMAKE_SOURCE_DIR})
        # _deps targets (GoogleTest's gtest_main, etc.) ship coverage
        # noise the project's report does not want — strip any binary
        # whose path lives under the build's _deps/ tree. The result is
        # joined into a space-separated string so the bash -c
        # invocation below receives a single, predictable argv slot.
        set(_filtered_str "")
        set(_skip FALSE)
        foreach(_token IN LISTS _objs)
            if(_skip)
                if(NOT _token MATCHES "_deps/")
                    set(_filtered_str "${_filtered_str} -object ${_token}")
                endif()
                set(_skip FALSE)
                continue()
            endif()
            if(_token STREQUAL "-object")
                set(_skip TRUE)
                continue()
            endif()
        endforeach()

        add_custom_target(coverage_report
            COMMAND ${CMAKE_COMMAND} -E rm -f
                    ${CMAKE_BINARY_DIR}/coverage/argos-coverage.profdata
            COMMAND bash -c "shopt -s nullglob; \
                files=(${CMAKE_BINARY_DIR}/coverage/*.profraw); \
                [[ -f ${CMAKE_BINARY_DIR}/default.profraw ]] && \
                    files+=(${CMAKE_BINARY_DIR}/default.profraw); \
                test \${#files[@]} -gt 0 || \
                    { echo 'no .profraw files — run ctest first'; exit 1; }; \
                ${LLVM_PROFDATA} merge -sparse \"\${files[@]}\" \
                    -o ${CMAKE_BINARY_DIR}/coverage/argos-coverage.profdata"
            COMMAND bash -c "${LLVM_COV} export -format=text -summary-only \
                    ${_filtered_str} \
                    -instr-profile=${CMAKE_BINARY_DIR}/coverage/argos-coverage.profdata \
                    -ignore-filename-regex='(_deps|build|tests/)' \
                    > ${CMAKE_BINARY_DIR}/coverage/coverage.json"
            COMMAND ${CMAKE_COMMAND} -E echo
                    "coverage report at ${CMAKE_BINARY_DIR}/coverage/coverage.json"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating coverage report"
            VERBATIM
        )
    endfunction()
endif()
