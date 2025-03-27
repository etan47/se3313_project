#ifndef DRAWWORKER_H
#define DRAWWORKER_H

#include "../PixelBuffer/pixelbuffer.h"
#include "../Board/pixel.h"
#include <vector>
#include <thread>
#include "../nlohmann/json.hpp"

class DrawWorker {
private:
    PixelBuffer* pixelBuffer;
    nlohmann::json req_json;
    std::vector<Pixel> pixels;
    std::thread workerThread;

    void run();

public:
    DrawWorker(PixelBuffer* pb, nlohmann::json req_json);
    void start();
};

#endif // DRAWWORKER_H