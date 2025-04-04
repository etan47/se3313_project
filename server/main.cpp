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

//! S testing multiple canvases
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>
#include <map>
#include <cstring>
#include <random>
#include <unordered_set>

int generateRandomID()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<int> dis(100000, 999999);
    return dis(gen);
}

#define SOCKET_PATH_PREFIX "/tmp/session_"

unordered_set<int> session_sockets;

string getURI(){
    //grab URI from .env
    string URI;
    ifstream file(".env");
    getline(file, URI);
    file.close();
    return URI;


}
void *loopUpdateDB(void *arg){

    string URI = getURI();
    mongocxx::client client{mongocxx::uri{URI}};
    db= client["whiteboard"];
    board_collection= db["board"];

    while (true){
        sleep(60); //update db every set amount of time
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

        // Insert into MongoDB
        
        board_collection.update_one(
            bsoncxx::builder::stream::document{} 
                << "_id" << bsoncxx::oid("67eb041d28dd4abe4c000efb") 
                << bsoncxx::builder::stream::finalize,  // filter by id
            bsoncxx::builder::stream::document{} 
                << "$set" << document.view() 
                << bsoncxx::builder::stream::finalize,  // update db entry
            mongocxx::options::update{}.upsert(true) // Create if not found
        );

        cout << "Matrix updated in MongoDB!" <<endl;

    }
   
        
}

void start_child_process(int session_id)
{
    pid_t pid = fork();
    if (pid == 0)
    {
       

        string URI = getURI();
        //cout<<URI<<endl;
        mongocxx::client client{mongocxx::uri{URI}};
        db= client["whiteboard"];
        board_collection= db["board"];
        user_collection = db["users"];
        try
        {
          // Ping the database.
          const auto ping_cmd = bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("ping", 1));
          db.run_command(ping_cmd.view());
          cout << "Pinged your deployment. You successfully connected to MongoDB!" <<endl;
    
        }
        catch (const std::exception& e)
        {
          // Handle errors
          cout<< "Exception: " << e.what() << endl;
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

        pthread_t thread = pthread_create(&thread, NULL, loopGetBuffer, (void *)pb);

        pthread_t updateDB = pthread_create(&thread, NULL, loopUpdateDB, (void *)pb);

        while (true)
        {
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

            string response;

            switch (function)
            {
            case 0:
            {
                cout << "Drawing..." << endl;
                DrawWorker *worker = new DrawWorker(pb, received_json);
                worker->start();
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
            default:
                cout << "Unknown function: " << function << endl;
            }

            write(client_fd, response.c_str(), response.size());

            close(client_fd);
        }
    }
}

//!! E testing multiple canvases

int main()
{
   
        

    string URI = getURI();
    //cout<<URI<<endl;
    mongocxx::client client{mongocxx::uri{URI}};
    db= client["whiteboard"];
    board_collection= db["board"];
    user_collection = db["users"];

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

    svr.Post("/startWhiteboard", [&](const httplib::Request &, httplib::Response &res){
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

        //Insert into MongoDB
        try {
            cout << "Attempting to insert into MongoDB..." << endl;
        
            auto result = board_collection.insert_one(document.view());
        
            if (result) {
                cout << "Insertion successful. Inserted ID: " 
                          << result->inserted_id().get_oid().value.to_string() 
                          << endl;
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


        int session_id = generateRandomID();
        while (session_sockets.find(session_id) != session_sockets.end())
        {
            session_id = generateRandomID(); // Regenerate if ID already exists
        }
        session_sockets.insert(session_id); // Store the session ID

            start_child_process(session_id);
            json response_json = json::object();
            response_json["session_id"] = session_id;
            string response = response_json.dump();
            res.status = 200;
            res.set_header("Content-Type", "application/json");
            res.set_content(response, "application/json"); 
    });

    svr.Get("/getBoard", [](const httplib::Request &req, httplib::Response &res)
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
                //cout<<email<<password<<endl;
                // for (const auto &entry : users){
                //     if (entry.email ==email && entry.password==password){
                //         res.status=200;
                //         res.set_content(email+" logged in!", "text/plain");
                //         return;
                        
                //     }
                // }
    
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
                    cout<<"email not found"<<endl;
                    res.set_content("Email not found", "text/plain");
                    res.status=404;
                }
    
                // res.set_content("Incorrect Email or Password", "text/plain");
                // res.status=401;
    
                
            } catch (const json::parse_error& e) {
                cout << "JSON Parse Error: " << e.what() << endl;
                res.status = 400;  
                res.set_content("Invalid JSON", "text/plain");
            }
        });

        svr.Get("/getBoardFromDB", [](const httplib::Request &, httplib::Response &res)
        {
    
    
            bsoncxx::oid object_id("67eb041d28dd4abe4c000efb");  //hardcoded id
            bsoncxx::builder::stream::document filter_builder;
            filter_builder << "_id" << object_id;
        
            auto board = board_collection.find_one(filter_builder.view());
    
            string json_str = bsoncxx::to_json(board->view()); // Convert BSON to JSON string
            nlohmann::json board_json = nlohmann::json::parse(json_str); //parse json
           
    
            res.set_header("Content-Type", "application/json");
            res.set_content(board_json.dump(), "application/json"); 
        });
        svr.Post("/closeBoard", [](const httplib::Request &, httplib::Response &res)
        {
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
        
                // Insert into MongoDB
                
                board_collection.update_one(
                    bsoncxx::builder::stream::document{} 
                        << "_id" << bsoncxx::oid("67eb041d28dd4abe4c000efb") 
                        << bsoncxx::builder::stream::finalize,  // filter by id
                    bsoncxx::builder::stream::document{} 
                        << "$set" << document.view() 
                        << bsoncxx::builder::stream::finalize,  // update db entry
                    mongocxx::options::update{}.upsert(true) // Create if not found
                );
        
                cout << "Matrix updated before closing Board in MongoDB!" <<endl;
                res.set_content("Matrix updated in MongoDB!", "text/plain");
        
        });

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
        } 
    });

    std::cout << "Server is running on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}