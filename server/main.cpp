#include "httplib.h"
#include <iostream>

#include "./Board/board.h"
#include "./User/user.h"
#include "nlohmann/json.hpp"

#include "./PixelBuffer/pixelbuffer.h"
#include "./DrawWorker/drawworker.h"

using json = nlohmann::json;

using namespace std;

//! S testing multiple canvases
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>
#include <map>
#include <cstring>

#define SOCKET_PATH_PREFIX "/tmp/session_"

map<int, int> session_sockets;

void start_child_process(int session_id)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        string socket_path = SOCKET_PATH_PREFIX + to_string(session_id);

        int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un addr{}; //!  what is this {} notation?
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, socket_path.c_str());

        unlink(socket_path.c_str());
        bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
        listen(server_fd, 1); //! 1 connection at a time

        cout << "Session " << session_id << " running at " << socket_path << endl;

        while (true)
        {
            int client_fd = accept(server_fd, nullptr, nullptr);
            if (client_fd < 0)
                continue;

            char buffer[65536] = {0}; //! what is this notation?
            read(client_fd, buffer, sizeof(buffer));
            cout << "[Session " << session_id << "] Received: " << buffer << endl;

            string response = "{\"status\": \"processed\"}";
            write(client_fd, response.c_str(), response.size());

            close(client_fd);
        }
    }
}

//!! E testing multiple canvases

void *loopGetBuffer(void *arg)
{

    PixelBuffer *pb = (PixelBuffer *)arg;

    while (true)
    {
        vector<Pixel> pixels = pb->getBuffer();
        cout << "Removed " << pixels.size() << " pixels from buffer." << endl;

        for (const auto &entry : pixels)
        {
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

    svr.Post("/startWhiteboard", [&](const httplib::Request &, httplib::Response &res)
             {
                 int length = session_sockets.size();
                 session_sockets[length] = length;
                 start_child_process(length);
                 res.set_content("Started whiteboard!", "text/plain"); });

    svr.Post("/messageWhiteboard", [&](const httplib::Request &req, httplib::Response &res)
             {
        
        if (!req.has_param("session_id"))
        {
            res.status = 400;
            res.set_content("Missing session_id", "text/plain");
            return;
        }

        string session_id = req.get_param_value("session_id");

        string target_socket = SOCKET_PATH_PREFIX + session_id;
        int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, target_socket.c_str());

        if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        {
            try
            {
                json req_json = json::parse(req.body);
                cout << req_json << endl;
                string request = req_json.dump();
                write(client_fd, request.c_str(), request.size());

                char response[65536] = {0};
                read(client_fd, response, sizeof(response));
                cout << "[Main Server] Response: " << response << endl;

                //TODO how do I setup the canvas dependencies in the child process
                //TODO what do I need to pass to the json to indicate the function, how do I switch between them on the child process
            }
            catch (const json::parse_error &e)
            {
                cout << "JSON Parse Error: " << e.what() << endl;
                res.status = 400;
                res.set_content("Invalid JSON", "text/plain");
                return;
            } 
        }
    
        close(client_fd);
        res.set_content("Messaged whiteboard!", "text/plain"); });

    svr.Get("/getBoard", [](const httplib::Request &, httplib::Response &res)
            {
        vector<vector<int>> board = getBoard();
        json board_json = board;
        res.set_header("Content-Type", "application/json");
        res.set_content(board_json.dump(), "application/json"); });

    svr.Post("/drawLine", [&](const httplib::Request &req, httplib::Response &res)
             {
        try
        {
            json req_json = json::parse(req.body);

            DrawWorker *worker = new DrawWorker(pb, req_json);
            worker->start();

            res.set_content("Drawn!", "text/plain");
        }
        catch (const json::parse_error &e)
        {
            cout << "JSON Parse Error: " << e.what() << endl;
            res.status = 400;
            res.set_content("Invalid JSON", "text/plain");
        } });

    svr.Put("/clear", [&](const httplib::Request &req, httplib::Response &res)
            {
        clearBoard();
        res.status = 200;
        res.set_content("Board Cleared!", "text/plain");
        return; });

    svr.Post("/login", [&](const httplib::Request &req, httplib::Response &res)
             {
        string email;
        string password;

        try
        {
            json req_json = json::parse(req.body);
            cout << req_json << endl;
            if (req_json.contains("email") && req_json["email"].is_string())
            {
                email = req_json["email"];
            }
            if (req_json.contains("password") && req_json["password"].is_string())
            {
                password = req_json["password"];
            }
            cout << email << password << endl;
            for (const auto &entry : users)
            {
                if (entry.email == email && entry.password == password)
                {
                    res.status = 200;
                    res.set_content(email + " logged in!", "text/plain");
                    return;
                }
            }
            res.set_content("Incorrect Email or Password", "text/plain");
            res.status = 401;
        }
        catch (const json::parse_error &e)
        {
            cout << "JSON Parse Error: " << e.what() << endl;
            res.status = 400;
            res.set_content("Invalid JSON", "text/plain");
        } });

    svr.Post("/register", [&](const httplib::Request &req, httplib::Response &res)
             {
        string email;
        string username;
        string password;

        try
        {
            json req_json = json::parse(req.body);
            cout << req_json << endl;

            if (req_json.contains("email") && req_json["email"].is_string())
            {
                email = req_json["email"];
            }

            if (req_json.contains("username") && req_json["username"].is_string())
            {
                username = req_json["username"];
            }

            if (req_json.contains("password") && req_json["password"].is_string())
            {
                password = req_json["password"];
            }

            cout << email << username << password << endl;

            // Check if all components have been filled up (Fix this...)
            if (email.empty() || username.empty() || password.empty())
            {
                res.status = 400; // Bad request status code
                res.set_content("Please fill in all parts!", "text/plain");
                return;
            }

            // Check if the email already exists in the users list
            for (const auto &entry : users)
            {
                if (entry.email == email)
                {
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
        }
        catch (const json::parse_error &e)
        {
            cout << "JSON Parse Error: " << e.what() << endl;
            res.status = 400;
            res.set_content("Invalid JSON", "text/plain");
        } });

    std::cout << "Server is running on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}