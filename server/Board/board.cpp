#include <iostream>
#include <vector>
#include <string>
#include "pixel.h"

using namespace std;

// vector<vector<string>> board(10, vector<string>(10, "white")); // 10 x 10 board with all white
vector<vector<int>> board(300, vector<int>(300, 7)); // 10 x 10 board with all white

// string value is the colour. x and y are just in the indicies

// vector<vector<string>> getBoard()
vector<vector<int>> getBoard()
{
    return board;
}

void addPixel(Pixel p)
{
    board[p.x][p.y] = p.colour;
    // cout << "Added pixel to board: " << p.colour << endl;
}