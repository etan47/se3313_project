#!/bin/bash
# compile.sh
g++ -I./nlohmann main.cpp Board/board.cpp Board/pixel.cpp PixelBuffer/pixelbuffer.cpp DrawWorker/drawworker.cpp User/user.cpp -o server -pthread -std=c++17 -I/usr/local/include/mongocxx/v_noabi -I/usr/local/include/bsoncxx/v_noabi -L/usr/local/lib -lmongocxx -lbsoncxx
./server