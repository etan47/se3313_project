#include "drawworker.h"
#include <iostream>
#include <chrono>
#include <thread>

using namespace std;

DrawWorker::DrawWorker(PixelBuffer *pb, nlohmann::json req_json) : pixelBuffer(pb)
{
    // make a copy of the json object
    this->req_json = req_json;
    this->start(); // Start the thread immediately
}

DrawWorker::~DrawWorker()
{
    join();
}

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
}

void DrawWorker::join()
{
    if (workerThread.joinable())
    {
        workerThread.join();
    }
}