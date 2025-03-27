#include "pixelbuffer.h"
#include <iostream>

PixelBuffer::PixelBuffer() : head(0), tail(0), count(0), buffer(BUFFER_SIZE, Pixel(0, 0, "white")) {}

void PixelBuffer::writePixels(std::vector<Pixel> pixels) {
    std::unique_lock<std::mutex> lock(mtx);

    not_full.wait(lock, [&]() { return count + pixels.size() <= BUFFER_SIZE; });

    for (const auto& p : pixels) {
        buffer[tail] = p;
        tail = (tail + 1) % BUFFER_SIZE;
        count++;
        std::cout << "PIXELBUFFER: Added pixel to buffer: " << p.colour << std::endl;
    }

    not_empty.notify_all();
}

std::vector<Pixel> PixelBuffer::getBuffer() {
    std::unique_lock<std::mutex> lock(mtx);

    not_empty.wait(lock, [&]() { return count > 0; });

    std::vector<Pixel> pixels;
    for (int i = 0; i < count; i++) {
        pixels.push_back(buffer[head]);
        head = (head + 1) % BUFFER_SIZE;
    }
    count = 0;

    not_full.notify_all();

    return pixels;
}