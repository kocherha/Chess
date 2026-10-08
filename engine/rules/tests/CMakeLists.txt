add_executable(chess_rules_tests
    PerftTest.cpp
    PositionTest.cpp
    GameTest.cpp
)
target_link_libraries(chess_rules_tests PRIVATE chess_rules GTest::gtest_main)

# PRE_TEST: enumerate at test time rather than build time.
gtest_discover_tests(chess_rules_tests DISCOVERY_MODE PRE_TEST)
