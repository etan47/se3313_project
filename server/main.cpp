#include "httplib.h"
#include <iostream>

#include "./Board/board.h"
#include "nlohmann/json.hpp"

#include "./PixelBuffer/pixelbuffer.h"
#include "./DrawWorker/drawworker.h"

using json = nlohmann::json;

using namespace std;

void *loopGetBuffer(void *arg)
{
    PixelBuffer *pb = (PixelBuffer *)arg;

    while (true)
    {
        vector<Pixel> pixels = pb->getBuffer();
        cout << "Removed " << pixels.size() << " pixels from buffer." << endl;

        // cout << pixels.size() << endl;
        for (const auto &entry : pixels)
        {
            // cout << entry.colour << endl;
            addPixel(entry);
        }
    }
}

// string boardToString(vector<vector<string>> board)
string boardToString(vector<vector<int>> board)
{
    string result;
    for (const auto &row : board)
    {
        for (const auto &cell : row)
        {
            result += cell + " ";
        }
        result += "\n";
    }
    return result;
}

int main()
{
    PixelBuffer *pb = new PixelBuffer();

    pthread_t thread = pthread_create(&thread, NULL, loopGetBuffer, (void *)pb);

    httplib::Server svr;

    svr.Get("/", [](const httplib::Request &, httplib::Response &res)
            { res.set_content("Hello from cpp-httplib!", "text/plain"); });

    svr.Get("/getBoard", [](const httplib::Request &, httplib::Response &res)
            {
                vector<vector<int>> board = getBoard();
                
                vector<vector<int>> transposed(board[0].size(), vector<int>(board.size()));

                for (size_t i = 0; i < 300; ++i) {
                    for (size_t j = 0; j < 300; ++j) {
                      transposed[j][i] = board[i][j];
                    }
                  }
                

                // json board_json = board;
                json board_json = transposed;
                res.set_header("Content-Type", "application/json");
                res.set_content(board_json.dump(), "application/json"); });

    svr.Post("/addPixel", [](const httplib::Request &req, httplib::Response &res)
             {
                 // string x;
                 // string y;
                 // string colour;
                 // try{
                 //     json req_json = json::parse(req.body);
                 //     cout<<req_json<<endl;
                 //     if (req_json.contains("colour")&& req_json["colour"].is_string()) {
                 //         colour = req_json["colour"];
                 //     }
                 //     if (req_json.contains("x")&& req_json["x"].is_string()) {
                 //         x = req_json["x"];
                 //     }
                 //     if (req_json.contains("y")&& req_json["y"].is_string()) {
                 //         y = req_json["y"];
                 //     }
                 //     cout<<x<<y<<colour<<endl;

                 //     Pixel p(stoi(x),stoi(y),colour);
                 //     addPixel(p);
                 // } catch (const json::parse_error& e) {
                 //     cout << "JSON Parse Error: " << e.what() << endl;
                 //     res.status = 400;  // Bad Request
                 //     res.set_content("Invalid JSON", "text/plain");
                 // }

                 // res.set_content("Pixel x:"+x+" y:"+y+", colour:"+colour+" added!", "text/plain");
             });

    svr.Post("/drawLine", [&](const httplib::Request &req, httplib::Response &res)
             {
        try {
            json req_json = json::parse(req.body);
    
            DrawWorker* worker = new DrawWorker(pb, req_json);
            worker->start(); 
            // cout << "Drawn!" << endl;
    
            res.set_content("Drawn!", "text/plain");
        } catch (const json::parse_error& e) {
            cout << "JSON Parse Error: " << e.what() << endl;
            res.status = 400;  
            res.set_content("Invalid JSON", "text/plain");
        } });

    std::cout << "Server is running on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}