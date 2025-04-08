#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <mongocxx/exception/exception.hpp>

#include "httplib.h"
#include <iostream>
#include <fstream>

#include "./Board/board.h"
#include "./User/user.h"
#include "nlohmann/json.hpp"

#include "./PixelBuffer/pixelbuffer.h"
#include "./DrawWorker/drawworker.h"

using json = nlohmann::json;

using namespace std;

// Create an instance.
mongocxx::instance inst{};

mongocxx::database db;
mongocxx::collection board_collection;
mongocxx::collection user_collection;

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

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>
#include <map>
#include <unordered_set>
#include "./BidirectionalMap/bidirectionalmap.h"
#include <cstring>
#include <random>
#include <sys/wait.h>
#include <memory>
#include <chrono>

int generateRandomID()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<int> dis(100000, 999999);
    return dis(gen);
}

#define SOCKET_PATH_PREFIX "/tmp/session_"

map<pid_t, int> pid_to_session_id_map; // Map to store pid to session id mapping
BidirectionalMap db_se_map;

void sigchld_handler(int signum)
{
    // Wait for any child process to terminate and clean up the session ID
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG); // Non-blocking call

    cout << status << endl;

    int session_id = pid_to_session_id_map[pid]; // Get the session ID from the map
    pid_to_session_id_map.erase(pid);            // Remove the pid from the map

    if (pid > 0)
    {
        db_se_map.removeSeID(session_id);

        cout << "[Parent] Cleaned up session " << session_id << endl;
    }
}

string getURI()
{
    // grab URI from .env
    string URI;
    ifstream file(".env");
    getline(file, URI);
    file.close();
    return URI;
}

void *loopUpdateDB(void *arg)
{

    string *db_id = (string *)arg;

    string URI = getURI();
    mongocxx::client client{mongocxx::uri{URI}};
    db = client["whiteboard"];
    board_collection = db["board"];

    while (true)
    {
        sleep(60); // update db every set amount of time
        vector<vector<int>> board = getBoard();

        // Convert matrix to BSON

        bsoncxx::builder::stream::document document{};
        bsoncxx::builder::stream::array matrix_array{};

        for (const auto &row : board)
        {
            bsoncxx::builder::stream::array row_array{};
            for (const auto &value : row)
            {

                row_array << value;
            }
            matrix_array << row_array;
        }

        document << "matrix" << matrix_array;

        // Insert into MongoDB

        board_collection.update_one(
            bsoncxx::builder::stream::document{}
                << "_id" << bsoncxx::oid(*db_id)
                << bsoncxx::builder::stream::finalize, // filter by id
            bsoncxx::builder::stream::document{}
                << "$set" << document.view()
                << bsoncxx::builder::stream::finalize, // update db entry
            mongocxx::options::update{}.upsert(true)   // Create if not found
        );

        cout << "Matrix updated in MongoDB!" << endl;
    }
}

void start_child_process(int session_id)
{
    pid_t pid = fork();
    if (pid == 0)
    {

        string URI = getURI();
        // cout<<URI<<endl;
        mongocxx::client client{mongocxx::uri{URI}};
        db = client["whiteboard"];
        board_collection = db["board"];
        user_collection = db["users"];
        try
        {
            // Ping the database.
            const auto ping_cmd = bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("ping", 1));
            db.run_command(ping_cmd.view());
            cout << "Pinged your deployment. You successfully connected to MongoDB!" << endl;
        }
        catch (const std::exception &e)
        {
            // Handle errors
            cout << "Exception: " << e.what() << endl;
        }

        string socket_path = SOCKET_PATH_PREFIX + to_string(session_id);

        int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un addr{}; //!  what is this {} notation?
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, socket_path.c_str());

        unlink(socket_path.c_str());
        bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
        listen(server_fd, 128);

        cout << "Session " << session_id << " running at " << socket_path << endl;

        PixelBuffer *pb = new PixelBuffer();

        pthread_t getBufferThread, updateDBThread;

        pthread_create(&getBufferThread, NULL, loopGetBuffer, (void *)pb);

        string db_id = db_se_map.getDbID(session_id); // Get the db id from the session id
        string *db_id_ptr = new string(db_id);        // Create a pointer to the db id
        pthread_create(&updateDBThread, NULL, loopUpdateDB, (void *)db_id_ptr);

        const int TIMEOUT_SECONDS = 300;                                     // 5 minutes timeout
        const auto TIMEOUT_DURATION = std::chrono::seconds(TIMEOUT_SECONDS); // Use your TIMEOUT_SECONDS value
        auto last_relevant_activity_time = std::chrono::steady_clock::now();

        while (true)
        { // not working because of frontend fetching canvas data every second
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(server_fd, &read_fds);

            auto now = std::chrono::steady_clock::now();
            auto deadline = last_relevant_activity_time + TIMEOUT_DURATION;
            auto time_remaining = deadline - now;

            struct timeval timeout;
            if (time_remaining <= std::chrono::seconds(0))
            {
                timeout.tv_sec = 0;
                timeout.tv_usec = 0;
            }
            else
            {
                timeout.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(time_remaining).count();
                timeout.tv_usec = std::chrono::duration_cast<std::chrono::microseconds>(time_remaining % std::chrono::seconds(1)).count();
            }

            int activity = select(server_fd + 1, &read_fds, nullptr, nullptr, &timeout);
            if (activity < 0)
            {
                cout << "Error in select()" << endl;
                continue;
            }
            else if (activity == 0)
            {
                cout << "Timeout occurred. No data after " << TIMEOUT_SECONDS << " seconds." << endl;

                pthread_cancel(getBufferThread);
                pthread_cancel(updateDBThread);

                pthread_join(getBufferThread, NULL);
                pthread_join(updateDBThread, NULL);

                delete pb;
                delete db_id_ptr;

                close(server_fd);
                unlink(socket_path.c_str());

                cout << "Session " << session_id << " closed." << endl;

                exit(0); // Exit the child process
            }

            int client_fd = accept(server_fd, nullptr, nullptr);
            if (client_fd < 0)
                continue;

            uint64_t net_size;
            read(client_fd, &net_size, sizeof(net_size)); // Read the size first
            net_size = ntohl(net_size);                   // Convert from network byte order to host byte order

            string full_request;
            full_request.resize(net_size); // Resize the string to hold the incoming data
            ssize_t bytes_read = 0;

            while (bytes_read < net_size)
            {
                ssize_t r = read(client_fd, &full_request[bytes_read], net_size - bytes_read);
                if (r > 0)
                {
                    bytes_read += r;
                }
            }

            json received_json = json::parse(full_request);

            int function = received_json["function"];

            if (function != 1)
            {
                last_relevant_activity_time = std::chrono::steady_clock::now();
            }

            string response;

            switch (function)
            {
            case 0:
            {
                cout << "Drawing..." << endl;
                DrawWorker *worker = new DrawWorker(pb, received_json);
                response = "{\"status\": \"drawing\"}";
                break;
            }
            case 1:
            {
                // cout << "Getting board..." << endl;
                vector<vector<int>> board = getBoard();
                json board_json = board;
                response = board_json.dump();
                break;
            }
            case 2:
            {
                cout << "Clearing board..." << endl;
                clearBoard();
                response = "{\"status\": \"cleared\"}";
                break;
            }
            case 3:
            {
                // load the matrix from the request
                cout << "Loading matrix..." << endl;

                cout << received_json["db_id"] << endl;

                // todo load the matrix from the db using the db_id
                string db_id = received_json["db_id"];

                try
                {
                    cout << "Attempting to load from MongoDB..." << endl;

                    auto result = board_collection.find_one(bsoncxx::builder::stream::document{} << "_id" << bsoncxx::oid(db_id) << bsoncxx::builder::stream::finalize);

                    if (result)
                    {
                        cout << "Retrieval successful. Found ID: "
                             << db_id << endl;

                        json matrix = json::array();                                   // Initialize an empty JSON array
                        auto matrix_bson = result->view()["matrix"].get_array().value; // Get the matrix from the BSON document
                        for (const auto &row : matrix_bson)
                        {
                            json row_json = json::array(); // Initialize an empty JSON array for each row
                            for (const auto &value : row.get_array().value)
                            {
                                row_json.push_back(value.get_int32().value); // Add the value to the row JSON array
                            }
                            matrix.push_back(row_json); // Add the row JSON array to the matrix JSON array
                        }

                        // create an object for each matrix value that holds colour, and an array of pixels, where each pixel is a pair of x and y coordinates
                        map<int, json> matrix_map;

                        for (int i = 0; i < matrix.size(); i++)
                        {
                            for (int j = 0; j < matrix[i].size(); j++)
                            {
                                int value = matrix[i][j].get<int>();

                                if (matrix_map.find(value) == matrix_map.end())
                                {
                                    matrix_map[value] = json::object();
                                    matrix_map[value]["colour"] = value;
                                    matrix_map[value]["pixels"] = json::array();
                                }

                                matrix_map[value]["pixels"].push_back(json::object());
                                matrix_map[value]["pixels"].back()["y"] = i; //! testing reverse coords
                                matrix_map[value]["pixels"].back()["x"] = j;
                            }
                        }

                        cout << "Matrix loaded successfully." << endl;

                        for (const auto &entry : matrix_map)
                        {
                            DrawWorker *worker = new DrawWorker(pb, entry.second);
                        }

                        response = "{\"status\": \"loaded\"}";

                        break;
                    }
                    else
                    {
                        cerr << "Search failed: No document was found." << endl;
                        response = "{\"status\": \"failed\"}";
                        break;
                    }
                }
                catch (const mongocxx::exception &e)
                {
                    cerr << "MongoDB Exception: " << e.what() << endl;
                }
                catch (const exception &e)
                {
                    cerr << "Standard Exception: " << e.what() << endl;
                }
                catch (...)
                {
                    cerr << "Unknown error occurred during insertion." << endl;
                }
            }
            default:
                cout << "Unknown function: " << function << endl;
            }

            write(client_fd, response.c_str(), response.size());

            close(client_fd);
        }
    }
    else if (pid > 0)
    {
        // Parent process
        pid_to_session_id_map[pid] = session_id; // Store the mapping of pid to session id
    }
}

int main()
{
    // Register signal handler for SIGCHLD
    struct sigaction sa;
    sa.sa_handler = sigchld_handler; // Specify handler function
    sa.sa_flags = SA_RESTART;        // Restart system calls if interrupted
    sigaction(SIGCHLD, &sa, NULL);   // Register the signal handler

    // handle kill signal
    signal(SIGINT, [](int signum)
           {
        cout << "Received SIGINT. Exiting..." << endl;
        // Clean up resources if needed
        // close all processes
        for (const auto &entry : pid_to_session_id_map)
        {
            pid_t pid = entry.first;
            int session_id = entry.second;
            kill(pid, SIGTERM); // Send SIGTERM to the child process
            cout << "Killed child process with PID: " << pid << " and session ID: " << session_id << endl;
        }
        exit(0); });

    string URI = getURI();
    // cout<<URI<<endl;
    mongocxx::client client{mongocxx::uri{URI}};
    db = client["whiteboard"];
    board_collection = db["board"];
    user_collection = db["users"];

    httplib::Server svr;

    svr.Get("/", [](const httplib::Request &, httplib::Response &res)
            { res.set_content("Hello from cpp-httplib!", "text/plain"); });

    svr.Post("/loadWhiteboard", [&](const httplib::Request &req, httplib::Response &res)
             {
                 if (!req.has_param("db_id"))
                 {
                     res.status = 400;
                     res.set_content("Missing db_id", "text/plain");
                     return;
                 }

                 string db_id = req.get_param_value("db_id");

                 if (db_se_map.containsDbID(db_id))
                 {
                     json response_json = json::object();
                     response_json["session_id"] = db_se_map.getSeID(db_id);
                     string response = response_json.dump();
                     res.status = 200;
                     res.set_header("Content-Type", "application/json");
                     res.set_content(response, "application/json");
                     return;
                 }

                 int session_id = generateRandomID();
                 while (db_se_map.containsSeID(session_id))
                 {
                     session_id = generateRandomID(); // Regenerate if ID already exists
                 }
                 db_se_map.insert(db_id, session_id); // Map the ID to the session ID

                 start_child_process(session_id);

                 {
                 this_thread::sleep_for(chrono::milliseconds(3000)); // Wait for the child process to start
                 
                string target_socket = SOCKET_PATH_PREFIX + to_string(session_id);

                 int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);

                 sockaddr_un addr{};
                 addr.sun_family = AF_UNIX;
                 strcpy(addr.sun_path, target_socket.c_str());

                 cout << "Connecting to child process socket: " << target_socket << endl;
                
                 if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
                 {
                    try {
                    json req_json = json::object();
                    req_json["function"] = 3; // Load matrix function
                    req_json["db_id"] = db_id; // Send the db_id to the child process
                    string request = req_json.dump();
                    size_t request_size = request.size();
                    uint64_t net_size = htonl(request_size); // Convert to network byte order
                    write(client_fd, &net_size, sizeof(net_size)); // Send the size first
                    write(client_fd, request.c_str(), request.size());

                    char response[65536] = {0};
                    read(client_fd, response, sizeof(response));
                    cout << "[Main Server] Response: " << response << endl;
                 }
                
                 catch (const json::parse_error &e)
                 {
                     cout << "JSON Parse Error: " << e.what() << endl;
                     res.status = 400;
                     res.set_content("Invalid JSON", "text/plain");
                     return;
                 }
                } 
                 else
                 {
                     cout << "Failed to connect to child process socket." << endl;
                     res.status = 500;
                     res.set_content("Server Error!", "text/plain");
                     return;
                 }
                
                 close(client_fd);
                }

                 json response_json = json::object();
                 response_json["session_id"] = session_id;
                 string response = response_json.dump();
                 res.status = 200;
                 res.set_header("Content-Type", "application/json");
                 res.set_content(response, "application/json"); });

    svr.Post("/startWhiteboard", [&](const httplib::Request &req, httplib::Response &res)
             {

                json req_json = json::parse(req.body);
                string email = req_json["email"];
                // cout << "Email: " << email << endl;

        vector<vector<int>> board = getBoard();

        // Convert matrix to BSON
    
        bsoncxx::builder::stream::document document{};
        bsoncxx::builder::stream::array matrix_array{};

        for (const auto& row : board) {
            bsoncxx::builder::stream::array row_array{};
            for (const auto& value : row) {
                
                row_array << value;
               
            }
            matrix_array << row_array;
        }

        document << "matrix" << matrix_array;

        string whiteboardID;

        //Insert into MongoDB
        try {
            cout << "Attempting to insert into MongoDB..." << endl;
        
            auto result = board_collection.insert_one(document.view());
        
            if (result) {
                whiteboardID = result->inserted_id().get_oid().value.to_string();
                cout << "Insertion successful. Inserted ID: " 
                          << whiteboardID << endl; 
            } else {
                cerr << "Insertion failed: No document was inserted." << endl;
            }
        } catch (const mongocxx::exception& e) {
            cerr << "MongoDB Exception: " << e.what() << endl;
        } catch (const exception& e) {
            cerr << "Standard Exception: " << e.what() << endl;
        } catch (...) {
            cerr << "Unknown error occurred during insertion." << endl;
        }

        try {
            cout << "Attempting to update user in MongoDB..." << endl;

            auto filter = bsoncxx::builder::stream::document{} << "email" << email << bsoncxx::builder::stream::finalize; // filter by email

            auto update = bsoncxx::builder::stream::document{} << "$addToSet" << bsoncxx::builder::stream::open_document
                << "whiteboards" << whiteboardID // add the whiteboard id to the user collection
                << bsoncxx::builder::stream::close_document << bsoncxx::builder::stream::finalize;
        
            auto result = user_collection.update_one(filter.view(), update.view());
        
            if (result) {
                cout << "User updated successfully." << endl;
            } else {
                cerr << "Update failed: No document was updated." << endl;
            }
        } catch (const mongocxx::exception& e) {
            cerr << "MongoDB Exception: " << e.what() << endl;
        } catch (const exception& e) {
            cerr << "Standard Exception: " << e.what() << endl;
        } catch (...) {
            cerr << "Unknown error occurred during insertion." << endl;
        }

        int session_id = generateRandomID();
        while (db_se_map.containsSeID(session_id))
        {
            session_id = generateRandomID(); // Regenerate if ID already exists
        }
        db_se_map.insert(whiteboardID, session_id); // Map the ID to the session ID

            start_child_process(session_id);
            json response_json = json::object();
            response_json["session_id"] = session_id;
            string response = response_json.dump();
            res.status = 200;
            res.set_header("Content-Type", "application/json");
            res.set_content(response, "application/json"); });

    svr.Post("/joinWhiteboard", [&](const httplib::Request &req, httplib::Response &res)
             {
        json req_json = json::parse(req.body);
        string email = req_json["email"];
        string session_id = req_json["session_id"];

        string whiteboardID = db_se_map.getDbID(stoi(session_id)); // Get the db id from the session id

        try {
            cout << "Attempting to update user in MongoDB..." << endl;

            auto filter = bsoncxx::builder::stream::document{} << "email" << email << bsoncxx::builder::stream::finalize; // filter by email

            auto update = bsoncxx::builder::stream::document{} << "$addToSet" << bsoncxx::builder::stream::open_document
                << "whiteboards" << whiteboardID // add the whiteboard id to the user collection
                << bsoncxx::builder::stream::close_document << bsoncxx::builder::stream::finalize;
        
            auto result = user_collection.update_one(filter.view(), update.view());
        
            if (result) {
                cout << "User updated successfully." << endl;
            } else {
                cerr << "Update failed: No document was updated." << endl;
            }
        } catch (const mongocxx::exception& e) {
            cerr << "MongoDB Exception: " << e.what() << endl;
        } catch (const exception& e) {
            cerr << "Standard Exception: " << e.what() << endl;
        } catch (...) {
            cerr << "Unknown error occurred during insertion." << endl;
        }

        res.status = 200; // Bad request status code
                res.set_content("Whiteboard Joined!", "text/plain");
                return; });

    svr.Get("/getWhiteboards", [&](const httplib::Request &req, httplib::Response &res)
            {
        if (!req.has_param("email"))
        {
            res.status = 400;
            res.set_content("Missing email", "text/plain");
            return;
        }

        string email = req.get_param_value("email");

        try {
            auto filter = bsoncxx::builder::stream::document{} << "email" << email << bsoncxx::builder::stream::finalize; // filter by email
            auto result = user_collection.find_one(filter.view());

            if (result) {
                auto doc_view = result->view();
                auto whiteboards_element = doc_view["whiteboards"];
            
                if (whiteboards_element && whiteboards_element.type() == bsoncxx::type::k_array) {
                    auto whiteboards_array = whiteboards_element.get_array().value;

                    json whiteboards_json = json::parse(bsoncxx::to_json(whiteboards_array));
                    json response_json = json::object();
                    response_json["whiteboards"] = whiteboards_json;
                    
                    res.set_header("Content-Type", "application/json");
                    res.set_content(response_json.dump(), "application/json"); // Set the response content to the JSON string
                    return;

                } else {
                    std::cout << "No whiteboards array found." << std::endl;
                }
            } else {
                std::cout << "User not found." << std::endl;
            }

            res.status = 500; 
            res.set_content("Server Error!", "text/plain");
            return;

        } catch (const mongocxx::exception& e) {
            cerr << "MongoDB Exception: " << e.what() << endl;
        } catch (const exception& e) {
            cerr << "Standard Exception: " << e.what() << endl;
        } catch (...) {
            cerr << "Unknown error occurred during insertion." << endl;
        }

        res.status = 500; // Internal server error status code
        res.set_content("Server Error!", "text/plain"); });

    svr.Get("/getBoard", [](const httplib::Request &req, httplib::Response &res)
            {
        if (!req.has_param("session_id"))
        {
            res.status = 400;
            res.set_content("Missing session_id", "text/plain");
            return;
        }

        string session_id = req.get_param_value("session_id");

        if (!db_se_map.containsSeID(stoi(session_id)))
        {
            res.status = 400;
            res.set_content("Invalid session_id", "text/plain");
            return;
        }

        string target_socket = SOCKET_PATH_PREFIX + session_id;
        int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, target_socket.c_str());

        if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        {
            try
            {
                json req_json = json::object();
                req_json["function"] = 1;
                string request = req_json.dump();
                size_t request_size = request.size();
                uint64_t net_size = htonl(request_size); // Convert to network byte order
                write(client_fd, &net_size, sizeof(net_size)); // Send the size first
                write(client_fd, request.c_str(), request.size());

                string full_response;
                char response[65536] = {0};
                ssize_t bytes_read;

                while ((bytes_read = read(client_fd, response, sizeof(response))) > 0)
                {
                    full_response.append(response, bytes_read);
                }
                                
                close(client_fd);
                json response_json = json::parse(full_response);
                res.set_header("Content-Type", "application/json");
                res.set_content(response_json.dump(), "application/json");
                return;
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
        res.status = 500;
        res.set_content("Server Error!", "text/plain"); });

    svr.Post("/drawLine", [&](const httplib::Request &req, httplib::Response &res)
             {
        if (!req.has_param("session_id"))
        {
            res.status = 400;
            res.set_content("Missing session_id", "text/plain");
            return;
        }

        string session_id = req.get_param_value("session_id");

        if (!db_se_map.containsSeID(stoi(session_id)))
        {
            res.status = 400;
            res.set_content("Invalid session_id", "text/plain");
            return;
        }

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
                req_json["function"] = 0;
                string request = req_json.dump();
                size_t request_size = request.size();
                uint64_t net_size = htonl(request_size); // Convert to network byte order
                write(client_fd, &net_size, sizeof(net_size)); // Send the size first
                write(client_fd, request.c_str(), request.size());
                
                char response[65536] = {0};
                read(client_fd, response, sizeof(response));
                cout << "[Main Server] Response: " << response << endl;

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
        res.status = 200;
        res.set_content("Drew to whiteboard!", "text/plain"); });

    svr.Put("/clear", [&](const httplib::Request &req, httplib::Response &res)
            {

        if (!req.has_param("session_id"))
        {
            res.status = 400;
            res.set_content("Missing session_id", "text/plain");
            return;
        }

        string session_id = req.get_param_value("session_id");

        // if (session_sockets.find(stoi(session_id)) == session_sockets.end())
        // {
        //     res.status = 400;
        //     res.set_content("Invalid session_id", "text/plain");
        //     return;
        // }
        if (!db_se_map.containsSeID(stoi(session_id)))
        {
            res.status = 400;
            res.set_content("Invalid session_id", "text/plain");
            return;
        }

        string target_socket = SOCKET_PATH_PREFIX + session_id;
        int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, target_socket.c_str());

        if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        {
            try
            {
                json req_json = json::object();
                req_json["function"] = 2;
                string request = req_json.dump();
                size_t request_size = request.size();
                uint64_t net_size = htonl(request_size); // Convert to network byte order
                write(client_fd, &net_size, sizeof(net_size)); // Send the size first
                write(client_fd, request.c_str(), request.size());
                
                char response[65536] = {0};
                read(client_fd, response, sizeof(response));
                cout << "[Main Server] Response: " << response << endl;

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
        res.status = 200;
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
    
                bsoncxx::builder::stream::document filter_builder;
                filter_builder << "email" << email;
                filter_builder << "password" << password; 
            
                // Execute the query
                mongocxx::cursor cursor = user_collection.find(filter_builder.view());
                cout<<"moving"<<endl;
                if (cursor.begin()!=cursor.end()){
                    cout<<"Found"<<endl;
                    for (auto&& doc : cursor) {
                        cout << bsoncxx::to_json(doc) << endl;
                        res.set_content(bsoncxx::to_json(doc), "application/json");
                        res.status=200;
                    }
                    
                }else{ //entry not found
                    cout<<"Invalid Credentials"<<endl;
                    res.set_content("Invalid Credentials", "text/plain");
                    res.status=401;
                }
    
                
            } catch (const json::parse_error& e) {
                cout << "JSON Parse Error: " << e.what() << endl;
                res.status = 400;  
                res.set_content("Invalid JSON", "text/plain");
            } });

    svr.Get("/getBoardFromDB", [](const httplib::Request &, httplib::Response &res)
            {
    
    
            bsoncxx::oid object_id("67eb041d28dd4abe4c000efb");  //hardcoded id
            bsoncxx::builder::stream::document filter_builder;
            filter_builder << "_id" << object_id;
        
            auto board = board_collection.find_one(filter_builder.view());
    
            string json_str = bsoncxx::to_json(board->view()); // Convert BSON to JSON string
            nlohmann::json board_json = nlohmann::json::parse(json_str); //parse json
           
    
            res.set_header("Content-Type", "application/json");
            res.set_content(board_json.dump(), "application/json"); });
    svr.Post("/closeBoard", [](const httplib::Request &, httplib::Response &res)
             {
                 vector<vector<int>> board = getBoard();

                 // Convert matrix to BSON

                 bsoncxx::builder::stream::document document{};
                 bsoncxx::builder::stream::array matrix_array{};

                 for (const auto &row : board)
                 {
                     bsoncxx::builder::stream::array row_array{};
                     for (const auto &value : row)
                     {

                         row_array << value;
                     }
                     matrix_array << row_array;
                 }

                 document << "matrix" << matrix_array;

                 // Insert into MongoDB

                 board_collection.update_one(
                     bsoncxx::builder::stream::document{}
                         << "_id" << bsoncxx::oid("67eb041d28dd4abe4c000efb")
                         << bsoncxx::builder::stream::finalize, // filter by id
                     bsoncxx::builder::stream::document{}
                         << "$set" << document.view()
                         << bsoncxx::builder::stream::finalize, // update db entry
                     mongocxx::options::update{}.upsert(true)   // Create if not found
                 );

                 cout << "Matrix updated before closing Board in MongoDB!" << endl;
                 res.set_content("Matrix updated in MongoDB!", "text/plain"); });

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

            // Check if all components have been filled up (Fix this...)
            if (email.empty() || username.empty() || password.empty())
            {
                res.status = 400; // Bad request status code
                res.set_content("Please fill in all parts!", "text/plain");
                return;
            }

            // Check if the email already exists in the database
            bsoncxx::builder::stream::document filter_builder;
            filter_builder << "email" << email;
            mongocxx::cursor cursor = user_collection.find(filter_builder.view());
            if (cursor.begin() != cursor.end())
            {
                res.status = 409; // Bad request status code
                res.set_content("Email already exists!", "text/plain");
                return;
            }
            
            // Insert the new user into the database
            bsoncxx::builder::stream::document document{};
            document << "email" << email
                     << "username" << username
                     << "password" << password
                     << "whiteboards" << bsoncxx::builder::stream::array{}; // Initialize an empty array for whiteboards
            user_collection.insert_one(document.view());

            // Check if the insertion was successful
            auto result = user_collection.find_one(filter_builder.view());
            if (!result)
            {
                res.status = 500; // Internal server error status code
                res.set_content("Failed to create account!", "text/plain");
                return;
            }
            // If the insertion was successful, you can return a success message or redirect the user
            // to a different page.


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