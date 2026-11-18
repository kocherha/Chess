# What the game's screens do, without the screens: solving a problem, going
# back over a game, the move list. No axmol here — the scenes in game/ show
# these and pass on what the player does — so all of it can be tested like
# the rules.
file(GLOB CHESS_SESSION_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(chess_session STATIC ${CHESS_SESSION_SOURCES})
target_include_directories(chess_session PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(chess_session PUBLIC cxx_std_20)
target_link_libraries(chess_session PUBLIC chess_rules chess_uci)
chess_sanitize(chess_session)
