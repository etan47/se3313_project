#ifndef PIXEL_H 
#define PIXEL_H

#include <string>

using namespace std;

class Pixel {
public:
    int x, y;
    string colour;

    Pixel(int x, int y, string colour);
};


#endif

