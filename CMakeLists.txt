cmake_minimum_required(VERSION 3.13)
project(Toka VERSION 0.3.0 LANGUAGES C CXX)

# Version Control Info
find_package(Git)
if(GIT_FOUND)
    # Get the commit count for build number
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE TOKA_BUILD_NUMBER
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    # Get the latest commit hash
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE TOKA_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(TOKA_BUILD_NUMBER 0)
    set(TOKA_COMMIT_HASH "unknown")
endif()

# Determine Target Triple
if(LLVM_DEFAULT_TARGET_TRIPLE)
    set(TOKA_TARGET_TRIPLE "${LLVM_DEFAULT_TARGET_TRIPLE}")
else()
    set(TOKA_TARGET_TRIPLE "${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_SYSTEM_NAME}")
endif()

# Configure the Version header
configure_file(
    ${CMAKE_SOURCE_DIR}/include/toka/Version.h.in
    ${CMAKE_SOURCE_DIR}/include/toka/Version.h
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find LLVM 20 intelligently
if(NOT LLVM_DIR)
    if(APPLE)
        # Check standard Homebrew locations for LLVM 20
        if(EXISTS "/opt/homebrew/opt/llvm@20/lib/cmake/llvm")
            set(LLVM_DIR "/opt/homebrew/opt/llvm@20/lib/cmake/llvm")
        elseif(EXISTS "/usr/local/opt/llvm@20/lib/cmake/llvm")
            set(LLVM_DIR "/usr/local/opt/llvm@20/lib/cmake/llvm")
        endif()
    elseif(UNIX AND NOT APPLE)
        # Check standard Ubuntu APT locations
        if(EXISTS "/usr/lib/llvm-20/lib/cmake/llvm")
            set(LLVM_DIR "/usr/lib/llvm-20/lib/cmake/llvm")
        endif()
    endif()
endif()
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

include_directories(include)

add_subdirectory(src)
