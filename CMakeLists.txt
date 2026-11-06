# DriftSpec — a workspace-level specification language and engine. Scaffolded by
# argos init --language cpp; the standard is raised to C++17: the tool is not
# ASIL-bound (argos.toml: NA) and argos-implementation-cpp14 reads C++17 fine
# (hyperstereo precedent).

cmake_minimum_required(VERSION 3.20)
project(driftspec LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()
add_compile_options(
  $<$<COMPILE_LANGUAGE:CXX>:-Wall> $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
  $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(argos-coverage OPTIONAL)
include(FetchContent)

# ---- third party: the tree-sitter runtime, JSON, GoogleTest -----------------
FetchContent_Declare(
  tree_sitter
  GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter.git
  GIT_TAG v0.25.3
  GIT_SHALLOW TRUE)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
  GIT_SHALLOW TRUE)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
  GIT_SHALLOW TRUE)
FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG v2.4.2
  GIT_SHALLOW TRUE)
set(JSON_BuildTests
    OFF
    CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json googletest cli11)
FetchContent_GetProperties(tree_sitter)
if(NOT tree_sitter_POPULATED)
  FetchContent_Populate(tree_sitter)
endif()
add_library(tree_sitter_runtime STATIC ${tree_sitter_SOURCE_DIR}/lib/src/lib.c)
target_include_directories(
  tree_sitter_runtime
  PUBLIC ${tree_sitter_SOURCE_DIR}/lib/include
  PRIVATE ${tree_sitter_SOURCE_DIR}/lib/src)

# ---- the grammar: generated parser.c + the external scanner ------------------
add_library(driftspec_grammar STATIC grammar/src/parser.c grammar/src/scanner.c)
target_include_directories(driftspec_grammar PRIVATE grammar/src)
target_link_libraries(driftspec_grammar PUBLIC tree_sitter_runtime)

# ---- the engine --------------------------------------------------------------
file(GLOB_RECURSE driftspec_SOURCES CONFIGURE_DEPENDS "src/ds/*.cpp")
# the viewer page ships inside ds: viewer/scene.html becomes a string the scene
# fills (ds view)
file(READ ${CMAKE_SOURCE_DIR}/viewer/scene.html DS_VIEWER_HTML)
file(
  WRITE ${CMAKE_BINARY_DIR}/generated/ViewerPage.cpp.in
  "namespace ds {\nnamespace scene {\nconst char* viewerTemplate() {\n  return R\"DSVIEW(${DS_VIEWER_HTML})DSVIEW\";\n}\n}  // namespace scene\n}  // namespace ds\n"
)
configure_file(${CMAKE_BINARY_DIR}/generated/ViewerPage.cpp.in
               ${CMAKE_BINARY_DIR}/generated/ViewerPage.cpp COPYONLY)
set_property(
  DIRECTORY
  APPEND
  PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/viewer/scene.html)
add_library(driftspec STATIC ${driftspec_SOURCES}
                             ${CMAKE_BINARY_DIR}/generated/ViewerPage.cpp)
target_include_directories(driftspec PUBLIC include)
target_link_libraries(driftspec PUBLIC driftspec_grammar
                                       nlohmann_json::nlohmann_json)

# ---- the CLI -----------------------------------------------------------------
add_executable(ds app/ds/main.cpp)
target_link_libraries(ds PRIVATE driftspec CLI11::CLI11)

# ---- tests: one executable per tests/unit/**/test_*.cpp (argos-test-gtest)
# ----
enable_testing()
file(GLOB_RECURSE driftspec_TESTS CONFIGURE_DEPENDS "tests/unit/*/test_*.cpp")
foreach(test_src IN LISTS driftspec_TESTS)
  get_filename_component(test_name ${test_src} NAME_WE)
  add_executable(${test_name} ${test_src})
  target_link_libraries(${test_name} PRIVATE driftspec GTest::gtest_main)
  target_compile_definitions(
    ${test_name} PRIVATE DRIFTSPEC_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples"
                         DRIFTSPEC_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# ---- integration tests: the engine over a workspace, one executable per
# tests/integration/**/test_*.cpp (argos-test-gtest) ----
enable_testing()
file(GLOB_RECURSE driftspec_INTEGRATION_TESTS CONFIGURE_DEPENDS
     "tests/integration/*/test_*.cpp")
foreach(test_src IN LISTS driftspec_INTEGRATION_TESTS)
  get_filename_component(test_name ${test_src} NAME_WE)
  add_executable(${test_name} ${test_src})
  target_link_libraries(${test_name} PRIVATE driftspec GTest::gtest_main)
  target_compile_definitions(
    ${test_name} PRIVATE DRIFTSPEC_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples"
                         DRIFTSPEC_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# ---- system qualification: tests/system/**/test_*.cpp drive the ds binary from
# outside ----
file(GLOB_RECURSE driftspec_SYSTEM_TESTS CONFIGURE_DEPENDS
     "tests/system/*/test_*.cpp")
foreach(test_src IN LISTS driftspec_SYSTEM_TESTS)
  get_filename_component(test_name ${test_src} NAME_WE)
  add_executable(${test_name} ${test_src})
  target_link_libraries(${test_name} PRIVATE nlohmann_json::nlohmann_json
                                             GTest::gtest_main)
  target_compile_definitions(
    ${test_name}
    PRIVATE DRIFTSPEC_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples"
            DRIFTSPEC_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
            DRIFTSPEC_DS_BIN="$<TARGET_FILE:ds>"
            DRIFTSPEC_BUILD_TYPE="${CMAKE_BUILD_TYPE}")
  add_dependencies(${test_name} ds)
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
