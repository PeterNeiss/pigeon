cmake_minimum_required (VERSION 3.20)

Include(CTest)
enable_testing()

add_executable(tests test.cpp)
target_link_libraries(tests PRIVATE 
  Catch2::Catch2WithMain
  pigeon::pigeon
)
add_test(NAME test COMMAND tests)

add_executable(iteration iteration.cpp)
target_link_libraries(iteration PRIVATE 
  Catch2::Catch2WithMain
  pigeon::pigeon
)
add_test(NAME iteration COMMAND iteration)
