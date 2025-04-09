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
        // cout << "Removed " << pixels.size() << " pixels from buffer." << endl;

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

// This function generates a random session ID between 100000 and 999999
int generateRandomID()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<int> dis(100000, 999999);
    return dis(gen);
}

#define SOCKET_PATH_PREFIX "/tmp/session_" // Prefix for the socket path

map<pid_t, int> pid_to_session_id_map; // Map to store pid to session id mapping
BidirectionalMap db_se_map;            // Map to store session ID to database ID mapping
map<int, json> board_cache;            // Map to store session ID to cached board mapping

// Function to shut down a child process gracefully
void sigchld_handler(int signum)
{
    // Wait for any child process to terminate and clean up the session ID
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG); // Non-blocking call

    int session_id = pid_to_session_id_map[pid]; // Get the session ID from the map
    pid_to_session_id_map.erase(pid);            // Remove the pid from the map

    // Check if the child process terminated successfully
    if (pid > 0)
    {
        db_se_map.removeSeID(session_id); // Remove the session ID from the database mapping
        board_cache.erase(session_id);    // Remove the session ID from the cache

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

// Function to start a board session in a child process
void start_child_process(int session_id)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        // Setup database connection in child process
        string URI = getURI();
        // cout<<URI<<endl;
        mongocxx::client client{mongocxx::uri{URI}};
        db = client["whiteboard"];
        board_collection = db["board"];
        user_collection = db["users"];

        // Ping the database to check connection
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

        // Create a server socket for the child process
        string socket_path = SOCKET_PATH_PREFIX + to_string(session_id);
        int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, socket_path.c_str());
        unlink(socket_path.c_str());
        bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
        listen(server_fd, 128);

        cout << "Session " << session_id << " running at " << socket_path << endl;

        // Create a PixelBuffer and threads for handling drawing and database updates
        PixelBuffer *pb = new PixelBuffer();

        // Thread Variables
        pthread_t getBufferThread, updateDBThread;

        pthread_create(&getBufferThread, NULL, loopGetBuffer, (void *)pb); // Create a thread to get the buffer

        string db_id = db_se_map.getDbID(session_id);                           // Get the db id from the session id
        string *db_id_ptr = new string(db_id);                                  // Create a pointer to the db id
        pthread_create(&updateDBThread, NULL, loopUpdateDB, (void *)db_id_ptr); // Create a thread to update the database

        // timeout variables
        const int TIMEOUT_SECONDS = 300;                                     // seconds
        const auto TIMEOUT_DURATION = std::chrono::seconds(TIMEOUT_SECONDS); // timeout duration
        auto last_relevant_activity_time = std::chrono::steady_clock::now(); // last relevant activity time

        // Main loop to handle incoming connections and requests
        while (true)
        {
            // File descriptor set for select
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(server_fd, &read_fds);

            // Set timeout for select
            auto now = std::chrono::steady_clock::now();
            auto deadline = last_relevant_activity_time + TIMEOUT_DURATION;
            auto time_remaining = deadline - now;

            // timeval structure for select timeout
            struct timeval timeout;
            // Set timeout to 0 if time_remaining is negative or zero
            if (time_remaining <= std::chrono::seconds(0))
            {
                timeout.tv_sec = 0;
                timeout.tv_usec = 0;
            }
            // Otherwise, set timeout to the remaining time
            else
            {
                timeout.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(time_remaining).count();
                timeout.tv_usec = std::chrono::duration_cast<std::chrono::microseconds>(time_remaining % std::chrono::seconds(1)).count();
            }

            // I/O block with timeout
            int activity = select(server_fd + 1, &read_fds, nullptr, nullptr, &timeout);
            // Error checking for select
            if (activity < 0)
            {
                cout << "Error in select()" << endl;
                continue;
            }
            // Timeout occurred
            else if (activity == 0)
            {
                cout << "Timeout occurred. No data after " << TIMEOUT_SECONDS << " seconds." << endl;

                // Cancel the threads
                pthread_cancel(getBufferThread);
                pthread_cancel(updateDBThread);

                // Join the threads
                pthread_join(getBufferThread, NULL);
                pthread_join(updateDBThread, NULL);

                // Clean up resources
                delete pb;
                delete db_id_ptr;

                // Close the server socket
                close(server_fd);
                unlink(socket_path.c_str());

                cout << "Session " << session_id << " closed." << endl;

                // Exit the child process
                exit(0);
            }

            // Accept a new connection
            int client_fd = accept(server_fd, nullptr, nullptr);
            // Error checking for accept
            if (client_fd < 0)
                continue;

            // recieve the request size
            uint64_t net_size;
            read(client_fd, &net_size, sizeof(net_size)); // Read the size first
            net_size = ntohl(net_size);                   // Convert from network byte order to host byte order

            // recieve the request
            string full_request;
            full_request.resize(net_size); // Resize the string to hold the incoming data
            ssize_t bytes_read = 0;

            // Read the request in a loop until all bytes are read
            while (bytes_read < net_size)
            {
                ssize_t r = read(client_fd, &full_request[bytes_read], net_size - bytes_read);
                if (r > 0)
                {
                    bytes_read += r;
                }
            }

            json received_json = json::parse(full_request); // Parse the JSON request

            int function = received_json["function"]; // Get the function from the request

            // If the function is not 1 (getBoard), update the last relevant activity time
            if (function != 1)
            {
                last_relevant_activity_time = std::chrono::steady_clock::now();
            }

            string response;

            // Handle the request based on the function
            switch (function)
            {
            //  0: Draw on the board
            case 0:
            {
                // Create a new DrawWorker to handle the drawing
                DrawWorker *worker = new DrawWorker(pb, received_json);
                response = "{\"status\": \"drawing\"}"; // Send a response indicating drawing is in progress
                break;
            }
            // 1: Get the board
            case 1:
            {
                // Get the board
                vector<vector<int>> board = getBoard();
                json board_json = board;
                response = board_json.dump(); // Send the board as a response
                break;
            }
            // 2: Clear the board
            case 2:
            {
                // Clear the board
                clearBoard();
                response = "{\"status\": \"cleared\"}"; // Send a response indicating the board has been cleared
                break;
            }
            // 3: Load the board from the database
            case 3:
            {
                string db_id = received_json["db_id"]; // Get the db_id from the request

                try
                {
                    cout << "Attempting to load from MongoDB..." << endl;

                    // Find the document in the collection using the db_id
                    auto result = board_collection.find_one(bsoncxx::builder::stream::document{} << "_id" << bsoncxx::oid(db_id) << bsoncxx::builder::stream::finalize);

                    // Check if the document was found
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

                        // Iterate through the matrix and populate the matrix_map
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
                                matrix_map[value]["pixels"].back()["y"] = i;
                                matrix_map[value]["pixels"].back()["x"] = j;
                            }
                        }

                        cout << "Matrix loaded successfully." << endl;

                        // Create a draw worker for each entry in the matrix_map
                        for (const auto &entry : matrix_map)
                        {
                            DrawWorker *worker = new DrawWorker(pb, entry.second);
                        }
                        response = "{\"status\": \"loaded\"}"; // Send a response indicating the board has been loaded
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
                response = "{\"status\": \"failed\"}"; // Send a response indicating the board load failed
                break;
            }
            default:
                cout << "Unknown function: " << function << endl;
                response = "{\"status\": \"unknown\"}"; // Send a response indicating an unknown function
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

    // Initialize MongoDB connection
    string URI = getURI();
    mongocxx::client client{mongocxx::uri{URI}};
    db = client["whiteboard"];
    board_collection = db["board"];
    user_collection = db["users"];

    httplib::Server svr; // Create an instance of the server
    // svr.set_keep_alive_max_count(100);
    // svr.set_keep_alive_timeout(20);

    // Root endpoint for testing
    svr.Get("/", [](const httplib::Request &, httplib::Response &res)
            { res.set_content("Hello from cpp-httplib!", "text/plain"); });

    // Endpoint to load the whiteboard
    svr.Post("/loadWhiteboard", [&](const httplib::Request &req, httplib::Response &res)
             {
                // check if the request has a db_id parameter
                 if (!req.has_param("db_id"))
                 {
                     res.status = 400;
                     res.set_content("Missing db_id", "text/plain");
                     return;
                 }

                 string db_id = req.get_param_value("db_id");

                 // check if a session already exists for this db_id
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

                 // create a new session with a random session id
                 int session_id = generateRandomID();
                 while (db_se_map.containsSeID(session_id))
                 {
                     session_id = generateRandomID(); // Regenerate if ID already exists
                 }
                 db_se_map.insert(db_id, session_id); // Map the ID to the session ID

                 start_child_process(session_id); // Start the child process

                 {
                 this_thread::sleep_for(chrono::milliseconds(3000)); // Wait for the child process to start
                 
                 // Create the client socket to connect to the child process
                string target_socket = SOCKET_PATH_PREFIX + to_string(session_id);
                 int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
                 sockaddr_un addr{};
                 addr.sun_family = AF_UNIX;
                 strcpy(addr.sun_path, target_socket.c_str());

                 cout << "Connecting to child process socket: " << target_socket << endl;
                
                 // Connect to the child process socket
                 if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
                 {
                    try {
                        // Create the JSON request to load the matrix
                        json req_json = json::object();
                        req_json["function"] = 3; // Load matrix function
                        req_json["db_id"] = db_id; // Send the db_id to the child process

                        // Send the request to the child process
                        string request = req_json.dump();
                        size_t request_size = request.size();
                        uint64_t net_size = htonl(request_size); // Convert to network byte order
                        write(client_fd, &net_size, sizeof(net_size)); // Send the size first
                        write(client_fd, request.c_str(), request.size()); // Send the request

                        // Receive the response from the child process
                        char response[65536] = {0};
                        read(client_fd, response, sizeof(response));
                        // cout << "[Main Server] Response: " << response << endl;
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

                // Send the response back to the client with the session ID
                 json response_json = json::object();
                 response_json["session_id"] = session_id;
                 string response = response_json.dump();
                 res.status = 200;
                 res.set_header("Content-Type", "application/json");
                 res.set_content(response, "application/json"); });

    // Endpoint to start a new whiteboard session
    svr.Post("/startWhiteboard", [&](const httplib::Request &req, httplib::Response &res)
             {
        
        // check if the request has an email parameter
        json req_json = json::parse(req.body);
        string email = req_json["email"];

        if (email.empty())
        {
            res.status = 400;
            res.set_content("Missing email", "text/plain");
            return;
        }

        // Get the default board
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
        //Insert whiteboard into MongoDB
        try {
            cout << "Attempting to insert into MongoDB..." << endl;
        
            auto result = board_collection.insert_one(document.view());
        
            if (result) {
                whiteboardID = result->inserted_id().get_oid().value.to_string(); // Get the inserted ID
                cout << "Insertion successful. Inserted ID: " 
                          << whiteboardID << endl; 
            } else {
                cerr << "Insertion failed: No document was inserted." << endl;
                res.status = 500; // Internal server error status code
                res.set_content("Server Error!", "text/plain");
                return;
            }
        } catch (const mongocxx::exception& e) {
            cerr << "MongoDB Exception: " << e.what() << endl;
            res.status = 500; // Internal server error status code
            res.set_content("Server Error!", "text/plain");
            return;
        } catch (const exception& e) {
            cerr << "Standard Exception: " << e.what() << endl;
            res.status = 500; // Internal server error status code
            res.set_content("Server Error!", "text/plain");
            return;
        } catch (...) {
            cerr << "Unknown error occurred during insertion." << endl;
            res.status = 500; // Internal server error status code
            res.set_content("Server Error!", "text/plain");
            return;
        }

        // add the whiteboard id to the user collection
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
                res.status = 500; // Internal server error status code
                res.set_content("Server Error!", "text/plain");
                return;
            }
        } catch (const mongocxx::exception& e) {
            cerr << "MongoDB Exception: " << e.what() << endl;
            res.status = 500; // Internal server error status code
            res.set_content("Server Error!", "text/plain");
            return;
        } catch (const exception& e) {
            cerr << "Standard Exception: " << e.what() << endl;
            res.status = 500; // Internal server error status code
            res.set_content("Server Error!", "text/plain");
            return;
        } catch (...) {
            cerr << "Unknown error occurred during insertion." << endl;
            res.status = 500; // Internal server error status code
            res.set_content("Server Error!", "text/plain");
            return;
        }

        // create a new session with a random session id
        int session_id = generateRandomID();
        while (db_se_map.containsSeID(session_id))
        {
            session_id = generateRandomID(); // Regenerate if ID already exists
        }
        db_se_map.insert(whiteboardID, session_id); // Map the ID to the session ID

        // start the child process
        start_child_process(session_id);

        // Send the response back to the client with the session ID
        json response_json = json::object();
        response_json["session_id"] = session_id;
        string response = response_json.dump();
        res.status = 200;
        res.set_header("Content-Type", "application/json");
        res.set_content(response, "application/json"); });

    // Endpoint to join an existing whiteboard session
    svr.Post("/joinWhiteboard", [&](const httplib::Request &req, httplib::Response &res)
             {
        
        // check if the request has an email and session_id paramete
        json req_json = json::parse(req.body);
        string email = req_json["email"];
        string session_id = req_json["session_id"];

        if (email.empty() || session_id.empty())
        {
            res.status = 400;
            res.set_content("Missing email or session_id", "text/plain");
            return;
        }

        string whiteboardID = db_se_map.getDbID(stoi(session_id)); // Get the db id from the session id

        // update the user collection with the whiteboard id
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
                res.status = 500; // Internal server error status code
                res.set_content("Server Error!", "text/plain");
                return;
            }
        } catch (const mongocxx::exception& e) {
            cerr << "MongoDB Exception: " << e.what() << endl;
            res.status = 500; // Internal server error status code
            res.set_content("Server Error!", "text/plain");
            return;
        } catch (const exception& e) {
            cerr << "Standard Exception: " << e.what() << endl;
            res.status = 500; // Internal server error status code
            res.set_content("Server Error!", "text/plain");
            return;
        } catch (...) {
            cerr << "Unknown error occurred during insertion." << endl;
            res.status = 500; // Internal server error status code
            res.set_content("Server Error!", "text/plain");
            return;
        }

        res.status = 200;
        res.set_content("Whiteboard Joined!", "text/plain");
        return; });

    // Endpoint to get the list of whiteboards for a user
    svr.Get("/getWhiteboards", [&](const httplib::Request &req, httplib::Response &res)
            {
        // check if the request has an email parameter
        if (!req.has_param("email"))
        {
            res.status = 400;
            res.set_content("Missing email", "text/plain");
            return;
        }

        string email = req.get_param_value("email");

        // get the users whiteboards from the database
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
                    res.status = 404; // Not found status code
                    res.set_content("No whiteboards found for this user.", "text/plain");
                    return;
                }
            } else {
                std::cout << "User not found." << std::endl;
                res.status = 404; // Not found status code
                res.set_content("User not found.", "text/plain");
                return;
            }

        } catch (const mongocxx::exception& e) {
            cerr << "MongoDB Exception: " << e.what() << endl;
        } catch (const exception& e) {
            cerr << "Standard Exception: " << e.what() << endl;
        } catch (...) {
            cerr << "Unknown error occurred during insertion." << endl;
        }

        res.status = 500; // Internal server error status code
        res.set_content("Server Error!", "text/plain"); });

    // Endpoint to get the board from the child process
    svr.Get("/getBoard", [](const httplib::Request &req, httplib::Response &res)
            {
        // check if the request has a session_id parameter
        if (!req.has_param("session_id"))
        {
            res.status = 400;
            res.set_content("Missing session_id", "text/plain");
            return;
        }

        string session_id = req.get_param_value("session_id");

        // check if the session id is valid
        if (!db_se_map.containsSeID(stoi(session_id)))
        {
            res.status = 400;
            res.set_content("Invalid session_id", "text/plain");
            return;
        }

        // check if board is cached in memory
        if (board_cache.find(stoi(session_id)) != board_cache.end())
        {
            json response_json = board_cache[stoi(session_id)];
            res.set_header("Content-Type", "application/json");
            res.set_content(response_json.dump(), "application/json");
            return;
        }

        // Create a client socket to connect to the child process
        string target_socket = SOCKET_PATH_PREFIX + session_id;
        int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, target_socket.c_str());

        // Connect to the child process socket
        if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        {
            try
            {
                // Create the JSON request to get the board
                json req_json = json::object();
                req_json["function"] = 1; // Get board function

                // Send the request to the child process
                string request = req_json.dump();
                size_t request_size = request.size();
                uint64_t net_size = htonl(request_size); // Convert to network byte order
                write(client_fd, &net_size, sizeof(net_size)); // Send the size first
                write(client_fd, request.c_str(), request.size()); // Send the request

                // Receive the response from the child process
                string full_response;
                char response[65536] = {0};
                ssize_t bytes_read;

                // Read the response in a loop until all bytes are read
                while ((bytes_read = read(client_fd, response, sizeof(response))) > 0)
                {
                    full_response.append(response, bytes_read);
                }
                                
                close(client_fd);
                
                json response_json = json::parse(full_response); // Parse the JSON response
                board_cache[stoi(session_id)] = response_json; // Cache the response

                // Send the response back to the client
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

    // Endpoint to draw on the board
    svr.Post("/drawLine", [&](const httplib::Request &req, httplib::Response &res)
             {
        // check if the request has a session_id parameter
        if (!req.has_param("session_id"))
        {
            res.status = 400;
            res.set_content("Missing session_id", "text/plain");
            return;
        }

        string session_id = req.get_param_value("session_id");

        // check if the session id is valid
        if (!db_se_map.containsSeID(stoi(session_id)))
        {
            res.status = 400;
            res.set_content("Invalid session_id", "text/plain");
            return;
        }

        // clear the cache for this session id
        if (board_cache.find(stoi(session_id)) != board_cache.end())
        {
            board_cache.erase(stoi(session_id));
        }

        // Create a client socket to connect to the child process
        string target_socket = SOCKET_PATH_PREFIX + session_id;
        int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, target_socket.c_str());

        // Connect to the child process socket
        if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        {
            try
            {
                // Create the JSON request to draw on the board
                json req_json = json::parse(req.body);
                req_json["function"] = 0; // Draw function

                // Send the request to the child process
                string request = req_json.dump();
                size_t request_size = request.size();
                uint64_t net_size = htonl(request_size); // Convert to network byte order
                write(client_fd, &net_size, sizeof(net_size)); // Send the size first
                write(client_fd, request.c_str(), request.size()); // Send the request
                
                // Receive the response from the child process
                char response[65536] = {0};
                read(client_fd, response, sizeof(response));
                // cout << "[Main Server] Response: " << response << endl;

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

    // Endpoint to clear the board
    svr.Put("/clear", [&](const httplib::Request &req, httplib::Response &res)
            {
        // check if the request has a session_id parameter
        if (!req.has_param("session_id"))
        {
            res.status = 400;
            res.set_content("Missing session_id", "text/plain");
            return;
        }

        string session_id = req.get_param_value("session_id");

        // check if the session id is valid
        if (!db_se_map.containsSeID(stoi(session_id)))
        {
            res.status = 400;
            res.set_content("Invalid session_id", "text/plain");
            return;
        }

        // clear the cache for this session id
        if (board_cache.find(stoi(session_id)) != board_cache.end())
        {
            board_cache.erase(stoi(session_id));
        }

        // Create a client socket to connect to the child process
        string target_socket = SOCKET_PATH_PREFIX + session_id;
        int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, target_socket.c_str());

        // Connect to the child process socket
        if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        {
            try
            {
                // Create the JSON request to clear the board
                json req_json = json::object();
                req_json["function"] = 2; // Clear function

                // Send the request to the child process
                string request = req_json.dump();
                size_t request_size = request.size();
                uint64_t net_size = htonl(request_size); // Convert to network byte order
                write(client_fd, &net_size, sizeof(net_size)); // Send the size first
                write(client_fd, request.c_str(), request.size()); // Send the request
                
                // Receive the response from the child process
                char response[65536] = {0};
                read(client_fd, response, sizeof(response));
                // cout << "[Main Server] Response: " << response << endl;

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

    // Endpoint to login a user
    svr.Post("/login", [&](const httplib::Request &req, httplib::Response &res)
             {
            string email;
            string password;
            
            try{
                // parse the request body
                json req_json = json::parse(req.body);
                if (req_json.contains("email")&& req_json["email"].is_string()) {
                    email = req_json["email"];
                }
                if (req_json.contains("password")&& req_json["password"].is_string()) {
                    password = req_json["password"];
                }

                // Check if all components are filled
                if (email.empty() || password.empty()) {
                    res.status = 400; // Bad request status code
                    res.set_content("Please fill in all parts!", "text/plain");
                    return;
                }
    
                // Create a filter to find the user with the given email and password
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

    // Endpoint to register a new user
    svr.Post("/register", [&](const httplib::Request &req, httplib::Response &res)
             {
        string email;
        string username;
        string password;

        try
        {
            // Parse the request body
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

            // Check if all components have been filled
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

    // Start the server and listen on port 8080
    std::cout << "Server is running on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}