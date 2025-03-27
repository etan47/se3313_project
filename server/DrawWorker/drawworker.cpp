#include "drawworker.h"
#include <iostream>
#include <chrono>
#include <thread>

DrawWorker::DrawWorker(PixelBuffer* pb, nlohmann::json req_json) : pixelBuffer(pb), req_json(req_json) {}

void DrawWorker::run() {
    for (auto& item : req_json["pixels"]) {
        int x = item["x"];
        int y = item["y"];
        std::string color = item["color"];
        pixels.emplace_back(x, y, color);

        // sleep for testing
        std::cout << "DRAWWORKER: x: " << x << " y: " << y << " color: " << color << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    if (!pixels.empty()) {
        pixelBuffer->writePixels(pixels);
    }
}

void DrawWorker::start() {
    workerThread = std::thread(&DrawWorker::run, this);
    workerThread.detach();
}