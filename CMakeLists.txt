cmake_minimum_required(VERSION 3.28)

project(cppgit)

set(CMAKE_CXX_STANDARD 17)

set(GIT2_INCLUDE_DIR "C:/Program Files (x86)/libgit2/include")
set(GIT2_LIBRARY_DIR "C:/Program Files (x86)/libgit2/lib")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
find_package(libgit2 REQUIRED)
include_directories(${GIT2_INCLUDE_DIR})
add_definitions(-DGIT_DEPRECATE_HARD)

add_executable(${PROJECT_NAME}
        main.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC "include/")

if(libgit2_FOUND)
    target_link_libraries(${PROJECT_NAME} ${GIT2_LIBRARIES})
endif()
