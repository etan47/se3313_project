#!/bin/bash
# compile.sh
g++ -I./nlohmann main.cpp Board/board.cpp Board/pixel.cpp PixelBuffer/pixelbuffer.cpp DrawWorker/drawworker.cpp -o server -pthread -std=c++11
./server