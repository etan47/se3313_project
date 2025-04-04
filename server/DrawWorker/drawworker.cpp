#include "drawworker.h"
#include <iostream>
#include <chrono>
#include <thread>

using namespace std;

DrawWorker::DrawWorker(PixelBuffer *pb, nlohmann::json req_json) : pixelBuffer(pb), req_json(req_json) {}

void DrawWorker::run()
{
    int colour = req_json["colour"];
    for (auto &item : req_json["pixels"])
    {
        int x = item["x"];
        int y = item["y"];
        pixels.emplace_back(x, y, colour);
    }

    if (!pixels.empty())
    {
        pixelBuffer->writePixels(pixels);
        cout << "Added " << pixels.size() << " pixels to buffer." << endl;
        cout << "Colour: " << colour << endl;
    }
}

void DrawWorker::start()
{
    workerThread = thread(&DrawWorker::run, this);
    workerThread.detach();
}