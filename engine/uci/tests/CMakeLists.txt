add_executable(chess_uci_tests
    DifficultyTest.cpp
    EngineTest.cpp
)
target_link_libraries(chess_uci_tests PRIVATE chess_uci chess_rules GTest::gtest_main)

# A stand-in engine: a shell script that speaks just enough UCI to exercise the
# client — the handshake, a move, a search that only ends when told to stop,
# and dying mid-game. It makes these tests exact and instant, and independent
# of whether Stockfish has been built.
target_compile_definitions(chess_uci_tests PRIVATE
    CHESS_FAKE_ENGINE="${CMAKE_CURRENT_SOURCE_DIR}/fake_engine.sh")

# The real thing, where the build puts it. Those tests skip themselves until
# it is there — that is, until the game has been built: the tests do not depend
# on Stockfish being compiled, so a tests-only configuration stays a matter of
# seconds.
target_compile_definitions(chess_uci_tests PRIVATE CHESS_REAL_ENGINE="${CHESS_REAL_ENGINE}")

gtest_discover_tests(chess_uci_tests DISCOVERY_MODE PRE_TEST)
