#include <iostream>
#include <vector>
#include <string>
#include "pixel.h"

using namespace std;

vector<vector<int>> board(300, vector<int>(300, 7)); // 300 x 300 board with all white

// string value is the colour. x and y are just in the indicies

vector<vector<int>> getBoard()
{
    return board;
}

void addPixel(Pixel p)
{
    board[p.x][p.y] = p.colour;
    // cout << "Added pixel to board: " << p.colour << endl;
}