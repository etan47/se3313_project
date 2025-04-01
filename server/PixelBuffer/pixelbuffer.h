#ifndef PIXELBUFFER_H
#define PIXELBUFFER_H

#include <vector>
#include <mutex>
#include <condition_variable>
#include "../Board/pixel.h"

class PixelBuffer
{
private:
    const int BUFFER_SIZE = 600 * 900; // 540000
    std::vector<Pixel> buffer;
    int head, tail, count;
    std::mutex mtx;
    std::condition_variable not_full, not_empty;

public:
    PixelBuffer();
    void writePixels(std::vector<Pixel> pixels);
    std::vector<Pixel> getBuffer();
};

#endif // PIXELBUFFER_H