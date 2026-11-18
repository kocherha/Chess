add_executable(chess_session_tests
    GameReviewTest.cpp
    MoveListTest.cpp
    OpponentTest.cpp
    PuzzleSessionTest.cpp
    WatcherTest.cpp
)
target_link_libraries(chess_session_tests PRIVATE chess_session GTest::gtest_main)

gtest_discover_tests(chess_session_tests DISCOVERY_MODE PRE_TEST)
