cmake_minimum_required(VERSION 3.10)
project(hermes_dart)

if(MSVC)
  set(CMAKE_CXX_STANDARD 20)
else()
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(HERMES_BUILD_SHARED_JS_ENGINE OFF CACHE BOOL "" FORCE)
set(HERMES_ENABLE_HERMES_ABI OFF CACHE BOOL "" FORCE)
set(HERMES_ENABLE_TEST_SUITE OFF CACHE BOOL "" FORCE)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(HERMES_ENABLE_TOOLS OFF CACHE BOOL "" FORCE)
else()
  set(HERMES_ENABLE_TOOLS ON CACHE BOOL "" FORCE)
endif()
set(HERMES_ENABLE_DEBUGGER OFF CACHE BOOL "" FORCE)
set(HERMES_ENABLE_NAPI OFF CACHE BOOL "" FORCE)
set(HERMES_ENABLE_CONTRIB_EXTENSIONS OFF CACHE BOOL "" FORCE)
if(WIN32)
  set(HERMES_CHECK_NATIVE_STACK OFF CACHE BOOL "" FORCE)
endif()

add_subdirectory(hermes)

add_library(hermes_dart SHARED 
    src/hermes_dart.cpp
)
get_directory_property(hermes_defs DIRECTORY hermes COMPILE_DEFINITIONS)
target_compile_definitions(hermes_dart PRIVATE ${hermes_defs})

# Must match hermesVMRuntime ABI: -fno-exceptions -fno-rtti
# Mismatch causes struct layout divergence and crashes.
if(MSVC)
  target_compile_options(hermes_dart PRIVATE
      /EHs-c-
      /GR-
      -wd4068
      -wd4200
      -wd4201
      -wd4530
      -wd4576
  )
else()
  target_compile_options(hermes_dart PRIVATE
      -fno-exceptions
      -fno-rtti
      -fvisibility=hidden
      -fno-strict-aliasing
      -fno-strict-overflow
      -Wno-invalid-offsetof
  )
endif()

if(WIN32)
  set_target_properties(hermes_dart PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

if(APPLE)
    target_link_libraries(hermes_dart PRIVATE 
        hermesvm_a
        hermescompiler 
        hermesInternalBytecode
        "-framework CoreFoundation"
    )
    target_link_options(hermes_dart PRIVATE 
        "-Wl,-force_load,${CMAKE_CURRENT_BINARY_DIR}/hermes/lib/libhermesvm_a.a"
        "-Wl,-undefined,dynamic_lookup" 
        "-Wl,-dead_strip"
    )
elseif(UNIX)
    # Use whole-archive to ensure all symbols from hermesvm_a are included.
    # We link hermesvm_a only once here wrapped in whole-archive to prevent multiple definition errors.
    target_link_libraries(hermes_dart PRIVATE 
        -Wl,--whole-archive hermesvm_a -Wl,--no-whole-archive
        hermescompiler 
        hermesInternalBytecode
    )
    target_link_options(hermes_dart PRIVATE "-Wl,-s" "-Wl,--gc-sections")
else() # Windows
    target_link_libraries(hermes_dart PRIVATE 
        hermesvm_a
        hermescompiler 
        hermesInternalBytecode
    )
endif()

target_include_directories(hermes_dart PUBLIC 
    hermes/include
    hermes/API
    ${CMAKE_CURRENT_BINARY_DIR}/hermes/include
    src
)

message(STATUS "Project ${PROJECT_NAME} configured with Hermes subdirectory")
