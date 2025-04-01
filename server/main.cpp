//mongodb includes

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>

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


// mongocxx::database db = client["whiteboard"];
// mongocxx::collection board_collection = db["board"];
// mongocxx::collection user_collection = db["user"];


mongocxx::database db;
mongocxx::collection board_collection;
mongocxx::collection user_collection;

    
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
        sleep(1); //update db every set amount of time
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

        std::cout << "Matrix inserted into MongoDB!" << std::endl;

    }
   
        
}

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
    string URI = getURI();
    
    mongocxx::client client{mongocxx::uri{URI}};
    db= client["whiteboard"];
    board_collection= db["board"];
    try
    {
      // Ping the database.
      const auto ping_cmd = bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("ping", 1));
      db.run_command(ping_cmd.view());
      std::cout << "Pinged your deployment. You successfully connected to MongoDB!" << std::endl;

    }
    catch (const std::exception& e)
    {
      // Handle errors
      std::cout<< "Exception: " << e.what() << std::endl;
    }

    User admin("admin@test.com","admin");
    User bob("bob@test.com","bob");

    vector<User> users;
    users.push_back(admin);
    users.push_back(bob);

    PixelBuffer *pb = new PixelBuffer();

    pthread_t thread = pthread_create(&thread, NULL, loopGetBuffer, (void *)pb);

    pthread_t updateDB = pthread_create(&thread, NULL, loopUpdateDB, (void *)pb);


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
        res.set_content(board_json.dump(), "application/json"); 
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

    svr.Post("/drawLine", [&](const httplib::Request &req, httplib::Response &res){
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