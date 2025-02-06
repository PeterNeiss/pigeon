cmake_minimum_required (VERSION 3.10)

add_executable (ticker1 cpp11/ticker/ticker1.cpp)
target_link_libraries(ticker1 PRIVATE pigeon::pigeon)

add_executable (ticker2 cpp11/ticker/ticker2.cpp)
target_link_libraries(ticker2 PRIVATE pigeon::pigeon)

add_executable (hover cpp11/hover/hover.cpp)
target_link_libraries(hover PRIVATE pigeon::pigeon)

add_subdirectory(cpp20)
