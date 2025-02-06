cmake_minimum_required (VERSION 3.10)

set (CMAKE_CXX_STANDARD 20)

add_executable (package1 package/package1.cpp)
target_link_libraries(package1 PRIVATE pigeon::pigeon)

add_executable (package2 package/package2.cpp)
target_link_libraries(package2 PRIVATE pigeon::pigeon)
