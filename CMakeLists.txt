cmake_minimum_required(VERSION 3.10)
project(IR_Search_Engine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")


find_package(mongocxx REQUIRED)
find_package(bsoncxx REQUIRED)

find_library(GUMBO_LIB NAMES gumbo gumbo-parser)

find_path(GUMBO_INCLUDE_DIR gumbo.h)

if(NOT GUMBO_LIB OR NOT GUMBO_INCLUDE_DIR)
    message(FATAL_ERROR "Gumbo library not found! Please install it.\nMacOS: brew install gumbo-parser\nUbuntu/Debian: sudo apt install libgumbo-dev")
else()
    message(STATUS "Found Gumbo lib: ${GUMBO_LIB}")
    message(STATUS "Found Gumbo headers: ${GUMBO_INCLUDE_DIR}")
endif()

file(GLOB_RECURSE STEMMER_SOURCES 
    "lib/libstemmer/src_c/*.c" 
    "lib/libstemmer/runtime/*.c"
    "lib/libstemmer/libstemmer/*.c"
)
if(NOT STEMMER_SOURCES)
    message(FATAL_ERROR "libstemmer sources not found! Check lib/libstemmer folder.")
endif()

add_library(stemmer_lib STATIC ${STEMMER_SOURCES})
target_include_directories(stemmer_lib PUBLIC lib/libstemmer/include)


file(GLOB_RECURSE ALL_SOURCES "src/*.cpp")


list(FILTER ALL_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")


add_library(core_lib STATIC ${ALL_SOURCES})

target_include_directories(core_lib PUBLIC 
    include 
    ${GUMBO_INCLUDE_DIR}
)


target_link_libraries(core_lib 
    PUBLIC 
    mongo::mongocxx_shared
    mongo::bsoncxx_shared
    stemmer_lib
    ${GUMBO_LIB}
)

add_executable(search_engine src/main.cpp)


target_link_libraries(search_engine PRIVATE core_lib)

enable_testing()
add_subdirectory(tests)