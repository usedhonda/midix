cmake_minimum_required(VERSION 3.15)
project(midix VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Options
option(MIDIX_BUILD_SHARED "Build shared library" ON)
option(MIDIX_BUILD_STATIC "Build static library" ON)
option(MIDIX_BUILD_CLI "Build CLI tool" ON)
option(MIDIX_BUILD_EXAMPLES "Build examples" ON)
option(MIDIX_BUILD_TESTS "Build tests" OFF)

# Platform detection
if(APPLE)
    set(MIDIX_PLATFORM_MAC ON)
elseif(WIN32)
    set(MIDIX_PLATFORM_WIN ON)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# Source files
set(MIDIX_COMMON_SOURCES
    src/common/midix_context.c
    src/common/midix_scheduler.c
    src/common/midix_util.c
    src/common/midix_api.c
)

if(MIDIX_PLATFORM_MAC)
    set(MIDIX_PLATFORM_SOURCES
        src/mac/midix_hal_mac.mm
    )
    set(MIDIX_PLATFORM_LIBS
        "-framework CoreMIDI"
        "-framework CoreAudio"
        "-framework CoreFoundation"
    )
elseif(MIDIX_PLATFORM_WIN)
    set(MIDIX_PLATFORM_SOURCES
        src/win/midix_hal_win.c
    )
    set(MIDIX_PLATFORM_LIBS
        winmm
    )
endif()

set(MIDIX_ALL_SOURCES
    ${MIDIX_COMMON_SOURCES}
    ${MIDIX_PLATFORM_SOURCES}
)

# Header files
set(MIDIX_PUBLIC_HEADERS
    include/midix.h
)

# Static library
if(MIDIX_BUILD_STATIC)
    add_library(midix_static STATIC ${MIDIX_ALL_SOURCES})
    target_include_directories(midix_static
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(midix_static PUBLIC ${MIDIX_PLATFORM_LIBS})
    set_target_properties(midix_static PROPERTIES
        OUTPUT_NAME midix
        PUBLIC_HEADER "${MIDIX_PUBLIC_HEADERS}"
    )

    if(MIDIX_PLATFORM_MAC)
        set_source_files_properties(src/mac/midix_hal_mac.mm PROPERTIES
            COMPILE_FLAGS "-fobjc-arc"
        )
    endif()
endif()

# Shared library
if(MIDIX_BUILD_SHARED)
    add_library(midix_shared SHARED ${MIDIX_ALL_SOURCES})
    target_include_directories(midix_shared
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(midix_shared PUBLIC ${MIDIX_PLATFORM_LIBS})
    set_target_properties(midix_shared PROPERTIES
        OUTPUT_NAME midix
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER "${MIDIX_PUBLIC_HEADERS}"
    )

    if(MIDIX_PLATFORM_MAC)
        set_source_files_properties(src/mac/midix_hal_mac.mm PROPERTIES
            COMPILE_FLAGS "-fobjc-arc"
        )
    endif()
endif()

# CLI tool
if(MIDIX_BUILD_CLI)
    add_executable(midix_cli
        cli/main.cpp
        cli/midix_parser.cpp
        cli/midix_commands.cpp
    )
    target_include_directories(midix_cli PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/cli
    )

    if(TARGET midix_static)
        target_link_libraries(midix_cli PRIVATE midix_static)
    else()
        target_link_libraries(midix_cli PRIVATE midix_shared)
    endif()

    set_target_properties(midix_cli PROPERTIES
        OUTPUT_NAME midix
    )
endif()

# Examples
if(MIDIX_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Benchmarks
option(MIDIX_BUILD_BENCHMARKS "Build benchmark programs" ON)
if(MIDIX_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Tests
if(MIDIX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
include(GNUInstallDirs)

if(MIDIX_BUILD_STATIC)
    install(TARGETS midix_static
        EXPORT midixTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(MIDIX_BUILD_SHARED)
    install(TARGETS midix_shared
        EXPORT midixTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(MIDIX_BUILD_CLI)
    install(TARGETS midix_cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# Export targets
install(EXPORT midixTargets
    FILE midixTargets.cmake
    NAMESPACE midix::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/midix
)

# Package config
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/midixConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/midixConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/midixConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/midix
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/midixConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/midixConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/midix
)
