include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
FetchContent_MakeAvailable(googletest)

file(GLOB TEST_SOURCES "*.cpp")

add_executable(run_tests ${TEST_SOURCES})

target_link_libraries(run_tests PRIVATE gtest_main core_lib)

target_include_directories(run_tests PRIVATE ../include)

include(GoogleTest)
gtest_discover_tests(run_tests)