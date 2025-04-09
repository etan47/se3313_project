#include "drawworker.h"
#include <iostream>
#include <chrono>
#include <thread>

using namespace std;

// Constructor for DrawWorker
DrawWorker::DrawWorker(PixelBuffer *pb, nlohmann::json req_json) : pixelBuffer(pb)
{
    this->req_json = req_json; // make a copy of the json object
    this->start();             // Start the thread immediately
}

// Destructor for DrawWorker
DrawWorker::~DrawWorker()
{
    join(); // Wait for the thread to finish before destroying the object
}

// Function to run in the thread
void DrawWorker::run()
{
    // Extract the pixels from the JSON object
    int colour = req_json["colour"];
    for (auto &item : req_json["pixels"])
    {
        int x = item["x"];
        int y = item["y"];
        pixels.emplace_back(x, y, colour);
    }

    if (!pixels.empty())
    {
        pixelBuffer->writePixels(pixels); // Write the pixels to the pixel buffer
        // cout << "Added " << pixels.size() << " pixels to buffer." << endl;
        // cout << "Colour: " << colour << endl;
    }
}

// Function to start the thread
void DrawWorker::start()
{
    workerThread = thread(&DrawWorker::run, this);
}

// Function to join the thread
void DrawWorker::join()
{
    if (workerThread.joinable())
    {
        workerThread.join();
    }
}