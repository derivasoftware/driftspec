# DriftSpec — a workspace-level specification language and engine. Scaffolded by
# argos init --language cpp; the standard is raised to C++17: the tool is not
# ASIL-bound (argos.toml: NA) and argos-implementation-cpp14 reads C++17 fine
# (hyperstereo precedent).

cmake_minimum_required(VERSION 3.20)
project(driftspec LANGUAGES C CXX)

option(DS_PYTHON "Build pyds._native, the engine as a Python extension (DS-84)"
       OFF)
if(DS_PYTHON)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON) # every static library ends up inside
                                          # a shared module
endif()

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
# the release archive: the repository's test data makes a clone 25 MB (DS-182)
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  URL_HASH
    SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
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
# miniz: zlib-compatible deflate, to pack the scene inside the viewer page (the
# browser inflates it natively)
FetchContent_Declare(
  miniz
  GIT_REPOSITORY https://github.com/richgel999/miniz.git
  GIT_TAG 3.0.2
  GIT_SHALLOW TRUE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5) # miniz still declares an old minimum,
                                      # which CMake 4 refuses
set(BUILD_EXAMPLES
    OFF
    CACHE BOOL "" FORCE)
set(BUILD_FUZZERS
    OFF
    CACHE BOOL "" FORCE)
set(BUILD_TESTS
    OFF
    CACHE BOOL "" FORCE)
set(INSTALL_PROJECT
    OFF
    CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json googletest cli11 miniz)
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
  "namespace ds {\nnamespace format {\nconst char* viewerTemplate() {\n  return R\"DSVIEW(${DS_VIEWER_HTML})DSVIEW\";\n}\n}  // namespace format\n}  // namespace ds\n"
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
target_link_libraries(
  driftspec
  PUBLIC driftspec_grammar nlohmann_json::nlohmann_json
  PRIVATE miniz)

# ---- the CLI -----------------------------------------------------------------
# ---- the Python face (pyds, DS-84): the engine as a CPython extension, abi3
# ----------
if(DS_PYTHON)
  find_package(Python 3.10 REQUIRED COMPONENTS Interpreter
                                               Development.SABIModule)
  python_add_library(_native MODULE USE_SABI 3.10 WITH_SOABI
                     python/src/native.cpp)
  target_link_libraries(_native PRIVATE driftspec)
  set_target_properties(_native PROPERTIES LIBRARY_OUTPUT_DIRECTORY
                                           ${CMAKE_BINARY_DIR}/python/pyds)
  if(SKBUILD) # pip install .: the module goes into the package of the wheel
    install(TARGETS _native LIBRARY DESTINATION pyds COMPONENT python)
  endif()
  add_custom_target(
    driftspec_python ALL # the package beside its native module, importable from
                         # the build
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/python/pyds
            ${CMAKE_BINARY_DIR}/python/pyds
    DEPENDS _native)
  # pytest drives pyds and its language server from the build (tests/*/python)
  enable_testing()
  add_test(
    NAME pyds
    COMMAND
      ${Python_EXECUTABLE} -m pytest -q -p no:cacheprovider
      ${CMAKE_SOURCE_DIR}/tests/integration/python
      ${CMAKE_SOURCE_DIR}/tests/system/python)
  set_tests_properties(pyds PROPERTIES ENVIRONMENT
                                       PYTHONPATH=${CMAKE_BINARY_DIR}/python)
endif()

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

# ---- the viewer's own code, run by node (DS-130): the tours it replays from
# what ds writes ----
find_program(DS_NODE node)
if(DS_NODE)
  add_test(NAME viewer_tours
           COMMAND ${DS_NODE} --test
                   ${CMAKE_SOURCE_DIR}/tests/viewer/test_tours.mjs)
  set_tests_properties(
    viewer_tours
    PROPERTIES
      ENVIRONMENT
      "DS=$<TARGET_FILE:ds>;VIEWER=${CMAKE_SOURCE_DIR}/viewer/scene.html;ROOT=${CMAKE_SOURCE_DIR}"
  )
else()
  # no node: the test fails rather than vanish, so a runner without it is
  # noticed
  add_test(NAME viewer_tours COMMAND ${CMAKE_COMMAND} -E false)
endif()
