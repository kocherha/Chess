#!/bin/sh
# A pretend UCI engine for the client's tests. See CMakeLists.txt.
#
#   go movetime 60000   reports on a search in progress, then "thinks" until it
#                       is told to stop and answers a1a2
#   go depth 99         exits without answering, as a crashed engine would
#   go ...              answers e2e4 at once, with a score
#
# Every setoption it receives is echoed back as an `info string`, so a test can
# see exactly what the client chose to send.

thinking=0

while IFS= read -r line; do
    case "$line" in
    uci)
        echo "id name Fake Engine 1.0"
        echo "id author nobody"
        echo "option name Threads type spin default 1 min 1 max 8"
        echo "option name Skill Level type spin default 20 min 0 max 20"
        echo "option name UCI_LimitStrength type check default false"
        echo "option name UCI_Elo type spin default 1320 min 1320 max 3190"
        echo "uciok"
        ;;
    isready)
        echo "readyok"
        ;;
    setoption*)
        echo "info string got $line"
        ;;
    ucinewgame)
        echo "info string got ucinewgame"
        ;;
    position*)
        echo "info string got $line"
        ;;
    "go movetime 60000"*)
        thinking=1
        # What a real engine prints on the way: chatter without a score, a
        # score that is only a bound, the lines for its second and third
        # choices, free text that happens to contain field names — and, in the
        # middle of all that, one line that actually says something.
        echo "info depth 9 currmove g1f3 currmovenumber 2"
        echo "info depth 9 seldepth 12 multipv 1 score cp 55 nodes 900 pv d2d4 d7d5 c2c4"
        echo "info depth 10 seldepth 14 multipv 1 score cp 140 lowerbound nodes 1500 pv d2d4"
        echo "info depth 9 seldepth 11 multipv 2 score cp -300 nodes 950 pv a2a3 e7e5"
        echo "info string depth 99 score mate 1 pv h2h4"
        ;;
    "go depth 99"*)
        exit 3
        ;;
    go*)
        echo "info depth 7 seldepth 9 score cp 31 nodes 1234 pv e2e4 e7e5"
        echo "bestmove e2e4 ponder e7e5"
        ;;
    stop)
        if [ "$thinking" = 1 ]; then
            thinking=0
            echo "info depth 3 score mate -2 pv a1a2"
            echo "bestmove a1a2"
        fi
        ;;
    quit)
        exit 0
        ;;
    esac
done
