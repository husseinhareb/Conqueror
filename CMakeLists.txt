cmake_minimum_required(VERSION 3.20)
project(rts_game LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Generate compile_commands.json for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Position Independent Code (required for shared libraries)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ============================================================================
# Godot CPP Setup
# ============================================================================

# Option to specify godot-cpp path
set(GODOT_CPP_DIR "${CMAKE_SOURCE_DIR}/godot-cpp" CACHE PATH "Path to godot-cpp repository")

# Check if godot-cpp exists
if(NOT EXISTS "${GODOT_CPP_DIR}/CMakeLists.txt")
    message(FATAL_ERROR 
        "godot-cpp not found at ${GODOT_CPP_DIR}\n"
        "Please clone godot-cpp:\n"
        "  git clone --recursive https://github.com/godotengine/godot-cpp.git ${GODOT_CPP_DIR}\n"
        "  cd ${GODOT_CPP_DIR} && git checkout godot-4.2-stable"
    )
endif()

# Add godot-cpp as subdirectory
add_subdirectory(${GODOT_CPP_DIR} godot-cpp)

# ============================================================================
# RTS Game Extension
# ============================================================================

# Source files
set(RTS_SOURCES
    src/RTSCamera.cpp
    src/Unit.cpp
    src/Building.cpp
    src/CommandCenter.cpp
    src/Barracks.cpp
    src/Vehicle.cpp
    src/Bulldozer.cpp
    src/FloorSnapper.cpp
    src/TerrainGenerator.cpp
    src/SelectionManager.cpp
    src/FlowFieldManager.cpp
    src/UnitSpawner.cpp
    src/GameManager.cpp
    src/RegisterExtensions.cpp
)

# Header files (for IDE support)
set(RTS_HEADERS
    include/RTSCamera.h
    include/Unit.h
    include/Building.h
    include/CommandCenter.h
    include/Barracks.h
    include/Vehicle.h
    include/Bulldozer.h
    include/FloorSnapper.h
    include/TerrainGenerator.h
    include/SelectionManager.h
    include/FlowFieldManager.h
    include/UnitSpawner.h
    include/GameManager.h
)

# Create the shared library
add_library(${PROJECT_NAME} SHARED ${RTS_SOURCES} ${RTS_HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${GODOT_CPP_DIR}/include
    ${GODOT_CPP_DIR}/gen/include
    ${GODOT_CPP_DIR}/gdextension
)

# Link godot-cpp
target_link_libraries(${PROJECT_NAME} PRIVATE godot-cpp)

# ============================================================================
# Output Configuration
# ============================================================================

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

# Platform-specific naming
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX "lib"
        SUFFIX ".linux.${CMAKE_BUILD_TYPE}.x86_64.so"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX ""
        SUFFIX ".windows.${CMAKE_BUILD_TYPE}.x86_64.dll"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX "lib"
        SUFFIX ".macos.${CMAKE_BUILD_TYPE}.universal.dylib"
    )
endif()

# ============================================================================
# Install (optional)
# ============================================================================

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION bin
    RUNTIME DESTINATION bin
)

# ============================================================================
# Print configuration
# ============================================================================

message(STATUS "")
message(STATUS "=== RTS Game GDExtension Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "godot-cpp path: ${GODOT_CPP_DIR}")
message(STATUS "Output directory: ${CMAKE_SOURCE_DIR}/bin")
message(STATUS "")
