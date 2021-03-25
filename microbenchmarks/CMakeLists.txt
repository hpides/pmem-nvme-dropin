cmake_minimum_required (VERSION 3.16.0)
project (mst_external_sort)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -g -funroll-loops -mmmx")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wall -Wpedantic -Wextra -std=c++17 -march=native")

# Eanble debug flags + santizer
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O2 -shared-libgcc -g -Wshadow -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wrestrict -Wnull-dereference -Wno-missing-braces -Wno-missing-field-initializers -Winline -Wundef -Wcast-qual -Wredundant-decls -Wunreachable-code -Wstrict-overflow=5")
#set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
#set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

include_directories(/mnt/nvme0/maximilian.boether/pmdk/pmdk/include)
link_directories(/mnt/nvme0/maximilian.boether/pmdk/pmdk/lib)

set(PROJECT_LINK_LIBS -lc)
if(UNIX AND NOT APPLE)
    set(PROJECT_LINK_LIBS ${PROJECT_LINK_LIBS} -lpmemlog -lpmem2)
endif()

include_directories(include)

file(GLOB  LibSources src/lib/*.cpp src/lib/**/*.cpp)
add_library(mst20-3 STATIC ${LibSources})

add_executable(bufman src/bin/bufman.cpp)
target_link_libraries(bufman mst20-3 ${PROJECT_LINK_LIBS})
add_dependencies(bufman mst20-3)
