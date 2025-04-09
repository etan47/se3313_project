#include "pixelbuffer.h"
#include <iostream>

// Constructor for PixelBuffer
PixelBuffer::PixelBuffer() : head(0), tail(0), count(0), buffer(BUFFER_SIZE, Pixel(0, 0, 7)) {}

// Thread-safe method to write pixels to the buffer
void PixelBuffer::writePixels(std::vector<Pixel> pixels)
{

    // Lock the mutex for thread safety
    std::unique_lock<std::mutex> lock(mtx);

    // Wait until there is enough space in the buffer
    not_full.wait(lock, [&]()
                  { return count + pixels.size() <= BUFFER_SIZE; });

    // Add pixels to the buffer
    for (const auto &p : pixels)
    {
        buffer[tail] = p;
        tail = (tail + 1) % BUFFER_SIZE;
        count++; // Increment the count of pixels in the buffer
    }

    // Notify any waiting threads that there are now pixels in the buffer
    not_empty.notify_all();
}

// Thread-safe method to get pixels from the buffer
std::vector<Pixel> PixelBuffer::getBuffer()
{

    // Lock the mutex for thread safety
    std::unique_lock<std::mutex> lock(mtx);

    // Wait until there are pixels in the buffer
    not_empty.wait(lock, [&]()
                   { return count > 0; });

    // Load pixels from the buffer into a vector
    std::vector<Pixel> pixels;
    for (int i = 0; i < count; i++)
    {
        pixels.push_back(buffer[head]);
        head = (head + 1) % BUFFER_SIZE;
    }

    count = 0; // clear the count of pixels in the buffer

    // Notify any waiting threads that there is now space in the buffer
    not_full.notify_all();

    // Return the loaded pixels
    return pixels;
}