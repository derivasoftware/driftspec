# DriftSpec — a workspace-level specification language and engine. Scaffolded by
# argos init --language cpp; the standard is raised to C++17: the tool is not
# ASIL-bound (argos.toml: NA) and argos-implementation-cpp14 reads C++17 fine
# (hyperstereo precedent).

cmake_minimum_required(VERSION 3.20)
project(driftspec LANGUAGES C CXX)

option(DS_PYTHON
       "Build driftspec._native, the engine as a Python extension (DS-84)" OFF)
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
if(MSVC)
  # the sources are UTF-8; the standard library's checked iterators and the
  # usual secure-CRT nagging stay as the platform has them
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/W4> /utf-8 /permissive-
                      /Zc:__cplusplus)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
  add_compile_options(
    $<$<COMPILE_LANGUAGE:CXX>:-Wall> $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
    $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>)
endif()
# Built with Emscripten (emcmake), the tree gives the engine for a page in a
# browser and nothing else (REQ-00113-1); the engine throws and catches, and
# WebAssembly exceptions keep that.
if(EMSCRIPTEN)
  add_compile_options(-fwasm-exceptions)
endif()

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
if(EMSCRIPTEN)
  FetchContent_MakeAvailable(nlohmann_json miniz)
else()
  FetchContent_MakeAvailable(nlohmann_json googletest cli11 miniz)
endif()
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
# the command line is not the engine: ds_app holds it (DS-311)
list(FILTER driftspec_SOURCES EXCLUDE REGEX "/src/ds/app/")
# A text file embedded in ds as a list of raw string literals of at most about 8
# KB each, cut at a line break (DS-283): MSVC refuses a literal over 16 KB and a
# concatenation over 64 KB, and the viewer page is more. The pieces are joined
# when the program runs. The text must not contain )DSX".
function(ds_embedded_pieces text out)
  set(rest "${text}")
  set(pieces "")
  while(NOT rest STREQUAL "")
    string(LENGTH "${rest}" length)
    if(length LESS_EQUAL 8000)
      set(cut ${length})
    else()
      string(SUBSTRING "${rest}" 0 8000 piece)
      string(FIND "${piece}" "\n" newline REVERSE)
      if(newline LESS 0)
        set(cut 8000)
      else()
        math(EXPR cut "${newline} + 1")
      endif()
    endif()
    string(SUBSTRING "${rest}" 0 ${cut} piece)
    string(SUBSTRING "${rest}" ${cut} -1 rest)
    string(APPEND pieces "R\"DSX(${piece})DSX\",\n")
  endwhile()
  set(${out}
      "${pieces}"
      PARENT_SCOPE)
endfunction()

# the viewer page ships inside ds: viewer/scene.html becomes a string the scene
# fills (ds view)
file(READ ${CMAKE_SOURCE_DIR}/viewer/scene.html DS_VIEWER_HTML)
ds_embedded_pieces("${DS_VIEWER_HTML}" DS_VIEWER_PIECES)
file(
  WRITE ${CMAKE_BINARY_DIR}/generated/ViewerPage.cpp.in
  "#include <string>\n\nnamespace ds {\nnamespace format {\nconst char* viewerTemplate() {\n  static const char* const pieces[] = {\n${DS_VIEWER_PIECES}  };\n  static const std::string whole = [] {\n    std::string out;\n    for (const char* piece : pieces) out += piece;\n    return out;\n  }();\n  return whole.c_str();\n}\n}  // namespace format\n}  // namespace ds\n"
)
configure_file(${CMAKE_BINARY_DIR}/generated/ViewerPage.cpp.in
               ${CMAKE_BINARY_DIR}/generated/ViewerPage.cpp COPYONLY)
set_property(
  DIRECTORY
  APPEND
  PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/viewer/scene.html)
# the guides of the manual ship inside ds too (DS-272): each
# doc/manual/NN-topic.md becomes a page ds manual <topic> prints and the site
# draws; one source for both
file(GLOB DS_MANUAL_GUIDES CONFIGURE_DEPENDS
     "${CMAKE_SOURCE_DIR}/doc/manual/*.md")
list(SORT DS_MANUAL_GUIDES)
set(DS_MANUAL_PAGES "")
foreach(guide ${DS_MANUAL_GUIDES})
  get_filename_component(topic ${guide} NAME_WE)
  string(REGEX REPLACE "^[0-9]+-" "" topic ${topic})
  file(READ ${guide} text)
  ds_embedded_pieces("${text}" pieces)
  string(APPEND DS_MANUAL_PAGES "    {\"${topic}\", {${pieces}}},\n")
  set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS ${guide})
endforeach()
file(
  WRITE ${CMAKE_BINARY_DIR}/generated/ManualGuides.cpp.in
  "#include <string>\n#include <utility>\n#include <vector>\n\nnamespace ds {\nnamespace model {\nconst std::vector<std::pair<std::string, std::string>>& manualGuides() {\n  struct Written {\n    const char* topic;\n    std::vector<const char*> pieces;\n  };\n  static const std::vector<Written> written = {\n${DS_MANUAL_PAGES}  };\n  static const std::vector<std::pair<std::string, std::string>> guides = [] {\n    std::vector<std::pair<std::string, std::string>> out;\n    for (const auto& w : written) {\n      std::string text;\n      for (const char* piece : w.pieces) text += piece;\n      out.emplace_back(w.topic, text);\n    }\n    return out;\n  }();\n  return guides;\n}\n}  // namespace model\n}  // namespace ds\n"
)
configure_file(${CMAKE_BINARY_DIR}/generated/ManualGuides.cpp.in
               ${CMAKE_BINARY_DIR}/generated/ManualGuides.cpp COPYONLY)
# the version of the tool, read from pyproject.toml where it is kept, said by ds
# --version, driftspec.__version__ and the export (DS-278)
file(STRINGS ${CMAKE_SOURCE_DIR}/pyproject.toml DS_VERSION_LINE
     REGEX "^version = \"[^\"]+\"")
string(REGEX REPLACE "^version = \"([^\"]+)\".*" "\\1" DS_VERSION
                     "${DS_VERSION_LINE}")
file(
  WRITE ${CMAKE_BINARY_DIR}/generated/Version.cpp.in
  "#include <string>\n\nnamespace ds {\nnamespace model {\nstd::string version() { return \"${DS_VERSION}\"; }\n}  // namespace model\n}  // namespace ds\n"
)
configure_file(${CMAKE_BINARY_DIR}/generated/Version.cpp.in
               ${CMAKE_BINARY_DIR}/generated/Version.cpp COPYONLY)
set_property(
  DIRECTORY
  APPEND
  PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/pyproject.toml)
add_library(
  driftspec STATIC
  ${driftspec_SOURCES} ${CMAKE_BINARY_DIR}/generated/ViewerPage.cpp
  ${CMAKE_BINARY_DIR}/generated/ManualGuides.cpp
  ${CMAKE_BINARY_DIR}/generated/Version.cpp)
target_include_directories(driftspec PUBLIC include)
target_link_libraries(
  driftspec
  PUBLIC driftspec_grammar nlohmann_json::nlohmann_json
  PRIVATE miniz)

# ---- the engine for a page in a browser (REQ-00113-1): ds.js and ds.wasm
# -------
if(EMSCRIPTEN)
  add_executable(ds_wasm app/wasm/main.cpp)
  target_link_libraries(ds_wasm PRIVATE driftspec)
  target_link_options(
    ds_wasm
    PRIVATE
    -fwasm-exceptions
    -sMODULARIZE=1
    -sEXPORT_ES6=1
    -sEXPORT_NAME=createDs
    -sENVIRONMENT=web,worker
    -sALLOW_MEMORY_GROWTH=1
    -sSTACK_SIZE=8MB # the design of driftspec itself recurses deeper than the
                     # default 64 KB (DS-236)
    -sEXPORTED_FUNCTIONS=_ds_playground
    -sEXPORTED_RUNTIME_METHODS=ccall)
  set_target_properties(ds_wasm PROPERTIES OUTPUT_NAME ds)
  # the grammar for the page's editor (DS-269): the parser as a module the
  # tree-sitter runtime of a browser loads, so the editor is coloured by the
  # same grammar and queries as every other editor
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/tree-sitter-driftspec.wasm
    COMMAND
      ${CMAKE_C_COMPILER} -o ${CMAKE_BINARY_DIR}/tree-sitter-driftspec.wasm -Os
      -fno-exceptions -fvisibility=hidden -sSIDE_MODULE=2
      -sEXPORTED_FUNCTIONS=_tree_sitter_driftspec
      -I${CMAKE_SOURCE_DIR}/grammar/src ${CMAKE_SOURCE_DIR}/grammar/src/parser.c
      ${CMAKE_SOURCE_DIR}/grammar/src/scanner.c
    DEPENDS ${CMAKE_SOURCE_DIR}/grammar/src/parser.c
            ${CMAKE_SOURCE_DIR}/grammar/src/scanner.c
    VERBATIM)
  add_custom_target(driftspec_grammar_wasm ALL
                    DEPENDS ${CMAKE_BINARY_DIR}/tree-sitter-driftspec.wasm)
  return()
endif()

# ---- the CLI -----------------------------------------------------------------
# ---- the Python face (import driftspec, DS-84, DS-288): the engine as a
# CPython extension, abi3
# ----------
if(DS_PYTHON)
  find_package(Python 3.10 REQUIRED COMPONENTS Interpreter
                                               Development.SABIModule)
  python_add_library(_native MODULE USE_SABI 3.10 WITH_SOABI
                     python/src/native.cpp)
  target_link_libraries(_native PRIVATE driftspec)
  set_target_properties(_native PROPERTIES LIBRARY_OUTPUT_DIRECTORY
                                           ${CMAKE_BINARY_DIR}/python/driftspec)
  if(SKBUILD) # pip install .: the module goes into the package of the wheel
    install(TARGETS _native LIBRARY DESTINATION driftspec COMPONENT python)
  endif()
  add_custom_target(
    driftspec_python ALL # the package beside its native module, importable from
                         # the build
    COMMAND
      ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/python/driftspec
      ${CMAKE_BINARY_DIR}/python/driftspec
    DEPENDS _native)
  # pytest drives the package and its language server from the build
  # (tests/*/python)
  enable_testing()
  add_test(
    NAME python
    COMMAND
      ${Python_EXECUTABLE} -m pytest -q -p no:cacheprovider
      ${CMAKE_SOURCE_DIR}/tests/integration/python
      ${CMAKE_SOURCE_DIR}/tests/system/python)
  set_tests_properties(python PROPERTIES ENVIRONMENT
                                         PYTHONPATH=${CMAKE_BINARY_DIR}/python)
endif()

# the engine's one call for a page as a shared library, for the playground's
# lesson replay (DS-204): built beside the Python face, whose static libraries
# are position independent
if(DS_PYTHON)
  add_library(ds_browser SHARED app/wasm/main.cpp)
  target_link_libraries(ds_browser PRIVATE driftspec)
endif()

find_package(Threads REQUIRED) # the work runs on a thread with a large stack
                               # (DS-279)
# the command line as functions (ds/app/App.hpp), a library of its own beside
# the engine so that its unit tests link it (DS-311)
add_library(ds_app STATIC src/ds/app/App.cpp)
target_link_libraries(
  ds_app
  PUBLIC driftspec
  PRIVATE CLI11::CLI11 Threads::Threads)
add_executable(ds app/ds/main.cpp)
target_link_libraries(ds PRIVATE ds_app)
if(SKBUILD) # pip install driftspec gives the ds command too, beside the package
            # (DS-303)
  install(TARGETS ds RUNTIME DESTINATION ${SKBUILD_SCRIPTS_DIR}
                             COMPONENT python)
endif()
if(MSVC)
  target_link_options(ds PRIVATE /STACK:536870912) # the 512 MB the work gets on
                                                   # POSIX (DS-283)
endif()

# ---- tests: one executable per tests/unit/**/test_*.cpp (argos-test-gtest)
# ----
enable_testing()
file(GLOB_RECURSE driftspec_TESTS CONFIGURE_DEPENDS "tests/unit/*/test_*.cpp")
foreach(test_src IN LISTS driftspec_TESTS)
  get_filename_component(test_name ${test_src} NAME_WE)
  add_executable(${test_name} ${test_src})
  target_link_libraries(${test_name} PRIVATE driftspec GTest::gtest_main)
  if(test_src MATCHES "/tests/unit/ds/app/") # the command line's functions
                                             # (DS-311)
    target_link_libraries(${test_name} PRIVATE ds_app)
  endif()
  target_compile_definitions(
    ${test_name}
    PRIVATE DRIFTSPEC_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples"
            DRIFTSPEC_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus"
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
    ${test_name}
    PRIVATE DRIFTSPEC_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples"
            DRIFTSPEC_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus"
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
            DRIFTSPEC_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus"
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
  # vitest (DS-329) comes from the package.json at the root: npm ci installs it
  add_test(
    NAME viewer_tours
    COMMAND ${DS_NODE} ${CMAKE_SOURCE_DIR}/node_modules/vitest/vitest.mjs run
            ${CMAKE_SOURCE_DIR}/tests/integration/viewer
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
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
