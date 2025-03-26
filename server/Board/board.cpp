
#include <vector>
#include <string>
#include "pixel.h"

using namespace std;


vector<vector<string>> board(10, vector<string>(10, "white")); //10 x 10 board with all white
//string value is the colour. x and y are just in the indicies

vector<vector<string>> getBoard(){
    return board;
}


void addPixel(Pixel p){
    board[p.x][p.y]= p.colour;
}