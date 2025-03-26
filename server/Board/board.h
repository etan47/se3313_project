#ifndef BOARD_H 
#define BOARD_H

#include <vector>
#include <string>
#include "pixel.h"

using namespace std;

vector<vector<string>> getBoard();
void addPixel(Pixel p);

#endif 