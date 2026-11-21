cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(nexus_core LANGUAGES CXX)

# Enforce C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "Configuring NEXUS C++ Engine (C++${CMAKE_CXX_STANDARD})")

# -----------------------------------------------------------------------------
# Discover LibTorch (PyTorch C++ Library)
# -----------------------------------------------------------------------------
# 1. Standard search via CMAKE_PREFIX_PATH / Torch_DIR
find_package(Torch QUIET)

# 2. Dynamic discovery via active Python environment torch.utils.cmake_prefix_path
if(NOT Torch_FOUND)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_FOUND)
    execute_process(
      COMMAND "${Python3_EXECUTABLE}" -c "import torch; print(torch.utils.cmake_prefix_path, end='')"
      OUTPUT_VARIABLE PYTHON_TORCH_PREFIX
      RESULT_VARIABLE PYTHON_TORCH_RES
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(PYTHON_TORCH_RES EQUAL 0 AND EXISTS "${PYTHON_TORCH_PREFIX}")
      message(STATUS "Discovered LibTorch via Python torch.utils.cmake_prefix_path: ${PYTHON_TORCH_PREFIX}")
      list(APPEND CMAKE_PREFIX_PATH "${PYTHON_TORCH_PREFIX}")
      find_package(Torch QUIET)
    endif()
  endif()
endif()

if(Torch_FOUND)
  message(STATUS "Found Torch: ${Torch_DIR} (version: ${Torch_VERSION})")
  add_compile_definitions(NEXUS_HAS_TORCH=1)
else()
  message(WARNING
    "\n================================================================================\n"
    "[NEXUS BUILD NOTICE] LibTorch / PyTorch C++ library not found!\n"
    "================================================================================\n"
    "The core engine target 'nexus_core' requires LibTorch to build.\n"
    "Test targets 'nexus_telemetry_test' and 'nexus_graph_test' will build without LibTorch.\n"
    "\n"
    "To build 'nexus_core' and enable LibTorch tensor tensors:\n"
    "  Option 1 (via pip / Python):\n"
    "    pip install torch\n"
    "    cmake -B build -DCMAKE_PREFIX_PATH=$(python3 -c 'import torch; print(torch.utils.cmake_prefix_path)')\n"
    "\n"
    "  Option 2 (Standalone LibTorch distribution):\n"
    "    Download from https://pytorch.org (macOS / C++ / Release)\n"
    "    cmake -B build -DCMAKE_PREFIX_PATH=/path/to/libtorch\n"
    "================================================================================\n"
  )
endif()

# -----------------------------------------------------------------------------
# Discover or Fetch nlohmann_json
# -----------------------------------------------------------------------------
find_package(nlohmann_json 3.2.0 QUIET)
if(nlohmann_json_FOUND)
  message(STATUS "Found nlohmann_json: ${nlohmann_json_DIR}")
elseif(TARGET nlohmann_json::nlohmann_json)
  message(STATUS "nlohmann_json target already present")
else()
  message(STATUS "nlohmann_json not found locally. Fetching via FetchContent (v3.11.3)...")
  include(FetchContent)
  FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()

# -----------------------------------------------------------------------------
# Target: nexus_core (requires LibTorch)
# -----------------------------------------------------------------------------
if(Torch_FOUND)
  add_executable(nexus_core
    src/main.cpp
    src/telemetry_collector.cpp
    src/graph_engine.cpp
  )

  target_include_directories(nexus_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${TORCH_INCLUDE_DIRS}
  )

  target_link_libraries(nexus_core PRIVATE
    ${TORCH_LIBRARIES}
    nlohmann_json::nlohmann_json
  )

  set_property(TARGET nexus_core PROPERTY CXX_STANDARD 17)
  set_property(TARGET nexus_core PROPERTY CXX_STANDARD_REQUIRED ON)
  set_property(TARGET nexus_core PROPERTY CXX_EXTENSIONS OFF)

  if(DEFINED TORCH_CXX_FLAGS)
    target_compile_options(nexus_core PRIVATE ${TORCH_CXX_FLAGS})
  endif()
endif()

# -----------------------------------------------------------------------------
# Target: nexus_telemetry_test (System 01 Test Suite)
# -----------------------------------------------------------------------------
add_executable(nexus_telemetry_test
  src/test_telemetry.cpp
  src/telemetry_collector.cpp
)

target_include_directories(nexus_telemetry_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(nexus_telemetry_test PRIVATE
  nlohmann_json::nlohmann_json
)

if(Torch_FOUND)
  target_include_directories(nexus_telemetry_test PRIVATE ${TORCH_INCLUDE_DIRS})
  target_link_libraries(nexus_telemetry_test PRIVATE ${TORCH_LIBRARIES})
  if(DEFINED TORCH_CXX_FLAGS)
    target_compile_options(nexus_telemetry_test PRIVATE ${TORCH_CXX_FLAGS})
  endif()
endif()

set_property(TARGET nexus_telemetry_test PROPERTY CXX_STANDARD 17)
set_property(TARGET nexus_telemetry_test PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET nexus_telemetry_test PROPERTY CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# Target: nexus_graph_test (System 02 Test Suite)
# -----------------------------------------------------------------------------
add_executable(nexus_graph_test
  src/test_graph.cpp
  src/graph_engine.cpp
  src/telemetry_collector.cpp
)

target_include_directories(nexus_graph_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(nexus_graph_test PRIVATE
  nlohmann_json::nlohmann_json
)

if(Torch_FOUND)
  target_include_directories(nexus_graph_test PRIVATE ${TORCH_INCLUDE_DIRS})
  target_link_libraries(nexus_graph_test PRIVATE ${TORCH_LIBRARIES})
  target_compile_definitions(nexus_graph_test PRIVATE NEXUS_HAS_TORCH=1)
  if(DEFINED TORCH_CXX_FLAGS)
    target_compile_options(nexus_graph_test PRIVATE ${TORCH_CXX_FLAGS})
  endif()
endif()

set_property(TARGET nexus_graph_test PROPERTY CXX_STANDARD 17)
set_property(TARGET nexus_graph_test PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET nexus_graph_test PROPERTY CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# CTest Enablement
# -----------------------------------------------------------------------------
enable_testing()
add_test(NAME nexus_telemetry_test COMMAND nexus_telemetry_test)
add_test(NAME nexus_graph_test COMMAND nexus_graph_test)
