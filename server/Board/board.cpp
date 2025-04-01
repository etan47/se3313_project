#include <iostream>
#include <vector>
#include <string>
#include "pixel.h"

using namespace std;

vector<vector<int>> board(600, vector<int>(600, 7)); // 600 x 600 board with all white

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

void clearBoard()
{
    board.assign(600, std::vector<int>(600, 7));
}