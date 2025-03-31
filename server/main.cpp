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

    User admin("admin@test.com", "admin");
    User bob("bob@test.com", "bob");

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
                json board_json = board;
                res.set_header("Content-Type", "application/json");
                res.set_content(board_json.dump(), "application/json"); });

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

    svr.Put("/clear", [&](const httplib::Request &req, httplib::Response &res)
            {
        clearBoard();
        res.status=200;
        res.set_content("Board Cleared!", "text/plain");
        return; });

    svr.Post("/login", [&](const httplib::Request &req, httplib::Response &res)
             {
        

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
        } });

    svr.Post("/register", [&](const httplib::Request &req, httplib::Response &res){
    
        string email;
        string username;
        string password;
    
        try{
            json req_json = json::parse(req.body);
            cout << req_json << endl;
        
            if (req_json.contains("email") && req_json["email"].is_string()) {
            email = req_json["email"];
            }
        
            if (req_json.contains("username") && req_json["username"].is_string()) {
            username = req_json["username"];
            }

            if (req_json.contains("password") && req_json["password"].is_string()) {
            password = req_json["password"];
            }
        
        cout << email << username << password << endl;
        
        // Check if all components have been filled up (Fix this...)
        if (!(email && username && password)){
            res.status = 408;
            res.set_content("Please fill in all parts!", "text/plain");
        }

        // Check if the email already exists in the users list
        for (const auto &entry : users) {
            if (entry.email == email) {
                // If the email already exists, return a message that account already exists
                res.status = 409; // Conflict status code
                res.set_content(email + " account already exists!", "text/plain");
                return;
            }
        }

        // If the email doesn't exist, create a new user
        // User new_user(email, username, password);
        // users.push_back(new_user);
        
        // Return success message...
        res.status = 201; // Created status code
        res.set_content(email + " created account!", "text/plain");
        
    } catch (const json::parse_error& e) {
        cout << "JSON Parse Error: " << e.what() << endl;
        res.status = 400;  
        res.set_content("Invalid JSON", "text/plain");
    }});

    std::cout << "Server is running on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}