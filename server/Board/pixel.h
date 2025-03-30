#ifndef PIXEL_H
#define PIXEL_H

#include <string>

using namespace std;

class Pixel
{
public:
    int x, y;
    // string colour;
    int colour;

    // Pixel(int x, int y, string colour);
    Pixel(int x, int y, int colour);
};

#endif
