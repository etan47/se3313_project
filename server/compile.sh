#!/bin/bash
# compile.sh
g++ -o server main.cpp -pthread -std=c++11
./server