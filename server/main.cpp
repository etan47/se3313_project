#include "httplib.h"
#include <iostream>

#include "./Board/board.h"
#include "nlohmann/json.hpp"

#include "./PixelBuffer/pixelbuffer.h"
#include "./DrawWorker/drawworker.h"

using json = nlohmann::json;

using namespace std;

string boardToString(vector<vector<string>> board) {
    string result;
    for (const auto& row : board) {
        for (const auto& cell : row) {
            result += cell + " ";
        }
        result += "\n";
    }
    return result;
}

int main() {
    PixelBuffer* pb = new PixelBuffer();
    httplib::Server svr;

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("Hello from cpp-httplib!", "text/plain");
    });

    svr.Get("/getBoard", [](const httplib::Request&, httplib::Response& res) {
       
        vector<vector<string>> board = getBoard();
        res.set_content(boardToString(board), "text/plain");
    });

    svr.Post("/addPixel", [](const httplib::Request& req, httplib::Response& res) {
       
        string x;
        string y;
        string colour;
        try{
            json req_json = json::parse(req.body);
            cout<<req_json<<endl;
            if (req_json.contains("colour")&& req_json["colour"].is_string()) {
                colour = req_json["colour"];  
            }
            if (req_json.contains("x")&& req_json["x"].is_string()) {
                x = req_json["x"];
            }
            if (req_json.contains("y")&& req_json["y"].is_string()) {
                y = req_json["y"];
            }
            cout<<x<<y<<colour<<endl;
           
            Pixel p(stoi(x),stoi(y),colour);
            addPixel(p);
        } catch (const json::parse_error& e) {
            cout << "JSON Parse Error: " << e.what() << endl;  
            res.status = 400;  // Bad Request
            res.set_content("Invalid JSON", "text/plain");
        }

        res.set_content("Pixel x:"+x+" y:"+y+", colour:"+colour+" added!", "text/plain");
    });

    svr.Post("/drawLine", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            json req_json = json::parse(req.body);
    
            DrawWorker* worker = new DrawWorker(pb, req_json);
            worker->start(); 
            cout << "Drawn!" << endl;
    
            res.set_content("Drawn!", "text/plain");
        } catch (const json::parse_error& e) {
            cout << "JSON Parse Error: " << e.what() << endl;
            res.status = 400;  
            res.set_content("Invalid JSON", "text/plain");
        }
    });

    std::cout << "Server is running on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}