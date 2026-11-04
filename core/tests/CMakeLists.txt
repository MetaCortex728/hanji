add_executable(hanji_tests
    main.cpp
    test_clock.cpp
    test_policy.cpp
    test_readers.cpp
    test_version.cpp)
target_link_libraries(hanji_tests PRIVATE
    hanji::core hanji_doctest hanji_json hanji_options)
target_compile_definitions(hanji_tests PRIVATE
    HANJI_EXPECTED_VERSION="${PROJECT_VERSION}"
    HANJI_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    HANJI_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")

add_test(NAME hanji_tests COMMAND hanji_tests)
