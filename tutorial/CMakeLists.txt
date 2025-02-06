cmake_minimum_required (VERSION 3.10)

add_executable(tutorial1 tutorial1.cpp)
target_link_libraries(tutorial1 PRIVATE pigeon::pigeon)

add_executable(tutorial2 tutorial2.cpp)
target_link_libraries(tutorial2 PRIVATE pigeon::pigeon)

add_executable(tutorial3 tutorial3.cpp)
target_link_libraries(tutorial3 PRIVATE pigeon::pigeon)

add_executable(tutorial4 tutorial4.cpp)
target_link_libraries(tutorial4 PRIVATE pigeon::pigeon)

add_executable(tutorial5 tutorial5.cpp)
target_link_libraries(tutorial5 PRIVATE pigeon::pigeon)
