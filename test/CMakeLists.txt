add_executable(tests tests.cpp)

target_link_libraries(tests PRIVATE sdb::libsdb Catch2::Catch2WithMain)

add_test(NAME tests COMMAND tests)

add_subdirectory(targets)
