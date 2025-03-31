#include "httplib.h"
#include <iostream>

#include "./Board/board.h"
#include "./User/user.h"
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

    User admin("admin@test.com","admin");
    User bob("bob@test.com","bob");

    vector<User> users;
    users.push_back(admin);
    users.push_back(bob);

    PixelBuffer *pb = new PixelBuffer();

    pthread_t thread = pthread_create(&thread, NULL, loopGetBuffer, (void *)pb);

    httplib::Server svr;

    svr.Get("/", [](const httplib::Request &, httplib::Response &res)
            { res.set_content("Hello from cpp-httplib!", "text/plain"); });

    svr.Get("/getBoard", [](const httplib::Request &, httplib::Response &res)
            {
                vector<vector<int>> board = getBoard();

                //TODO temporary transpose, we want this function to be quick, problem should be addressed elsewhere
                vector<vector<int>> transposed(board[0].size(), vector<int>(board.size()));
                for (size_t i = 0; i < 300; ++i) {
                    for (size_t j = 0; j < 300; ++j) {
                      transposed[j][i] = board[i][j];
                    }
                  }
                
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
    
            res.set_content("Drawn!", "text/plain");
        } catch (const json::parse_error& e) {
            cout << "JSON Parse Error: " << e.what() << endl;
            res.status = 400;  
            res.set_content("Invalid JSON", "text/plain");
        } });

    svr.Put("/clear", [&](const httplib::Request &req, httplib::Response &res){
        clearBoard();
        res.status=200;
        res.set_content("Board Cleared!", "text/plain");
        return;
    });

    svr.Post("/login", [&](const httplib::Request &req, httplib::Response &res){
        

        string email;
        string password;
        
        try{
            json req_json = json::parse(req.body);
            cout<<req_json<<endl;
            if (req_json.contains("email")&& req_json["email"].is_string()) {
                email = req_json["email"];
            }
            if (req_json.contains("password")&& req_json["password"].is_string()) {
                password = req_json["password"];
            }
            cout<<email<<password<<endl;
            for (const auto &entry : users){
                if (entry.email ==email && entry.password==password){
                    res.status=200;
                    res.set_content(email+" logged in!", "text/plain");
                    return;
                    
                }
            }
            res.set_content("Incorrect Email or Password", "text/plain");
            res.status=401;

            
        } catch (const json::parse_error& e) {
            cout << "JSON Parse Error: " << e.what() << endl;
            res.status = 400;  
            res.set_content("Invalid JSON", "text/plain");
        }});

    std::cout << "Server is running on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}