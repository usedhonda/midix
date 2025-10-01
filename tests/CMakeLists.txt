# Test suite configuration
# Uses Google Test framework

enable_testing()

# Fetch Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
  GIT_SHALLOW    TRUE
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# ============================================================================
# Test: Parser
# ============================================================================

add_executable(test_parser
  test_parser.cpp
  ${CMAKE_SOURCE_DIR}/cli/midix_parser.cpp
  ${CMAKE_SOURCE_DIR}/src/common/midix_util.c
)

target_include_directories(test_parser PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/cli
  ${CMAKE_SOURCE_DIR}/src/common
)

target_link_libraries(test_parser PRIVATE
  GTest::gtest_main
  midix_static
)

# Enable C++17 for tests
target_compile_features(test_parser PRIVATE cxx_std_17)

add_test(NAME test_parser COMMAND test_parser)

# ============================================================================
# Test: Utility Functions
# ============================================================================

add_executable(test_util
  test_util.cpp
)

target_include_directories(test_util PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src/common
)

target_link_libraries(test_util PRIVATE
  GTest::gtest_main
  midix_static
)

target_compile_features(test_util PRIVATE cxx_std_17)

add_test(NAME test_util COMMAND test_util)

# ============================================================================
# Test: Core API
# ============================================================================

add_executable(test_api
  test_api.cpp
)

target_include_directories(test_api PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_api PRIVATE
  GTest::gtest_main
  midix_static
)

target_compile_features(test_api PRIVATE cxx_std_17)

add_test(NAME test_api COMMAND test_api)

# ============================================================================
# Test Configuration
# ============================================================================

# Set test properties
set_tests_properties(test_parser test_util test_api PROPERTIES
  TIMEOUT 30
  LABELS "unit"
)

# Create a custom target to run all tests
add_custom_target(run_tests
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  DEPENDS test_parser test_util test_api
  COMMENT "Running all unit tests..."
)

# Add test labels for selective running
set_tests_properties(test_parser PROPERTIES LABELS "parser;cli")
set_tests_properties(test_util PROPERTIES LABELS "util;core")
set_tests_properties(test_api PROPERTIES LABELS "api;core")

# ============================================================================
# Notes
# ============================================================================

# Run specific test:
#   ctest -R test_parser -V
#
# Run by label:
#   ctest -L core -V
#
# Run all tests:
#   ctest --output-on-failure
#   # or
#   make run_tests
#
# Build and run:
#   make test_api && ./test_api
#
# Enable verbose output:
#   CTEST_OUTPUT_ON_FAILURE=1 make test
