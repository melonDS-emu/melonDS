#include <thread>
#include "../ARM.h"
#include<string>
#include <vector>
#include <stdexcept>
#include "http.h"


// https://stackoverflow.com/a/46943631, with warnings cleaned up and specialized for just slashes
std::vector<std::string> split(std::string str){
    std::vector<std::string>result;
    while(!str.empty()){
        std::string::size_type index = str.find('/');
        if(index!=std::string::npos){
            if(!str.substr(0,index).empty()) result.push_back(str.substr(0,index));
            str = str.substr(index + 1);
//            if(str.empty())result.push_back(str);
        }else{
            if(!str.empty()) result.push_back(str);
            str = "";
        }
    }
    return result;
}


/**
 * A basic HTTP server which responds with data values from NDS (todo: other engines)
 */
void rpc()
{
    std::thread([](){
        http(8080, [](request req){
            std::vector<std::string> path = split(req.path);
            std::string method = req.method;
            // TODO - one generic try catch here that throws a 500 rather than crashing
            // TODO - trying to read before booting causes crash
            // GET / - prints some basic data (todo: print that data)
            if(path.empty() && method == "GET") {
                return response{
                    .headers = std::map<std::string, std::string>{
                        {"Content-Type", "text/plain"}
                    },
                    .body = "Hello world!"
                };
            // GET /<region>>/<address>/<size> - Gets a memory address from that region
            } else if(path.size() == 3 && (path[0] == "arm7" || path[0] == "arm9")) {
                std::string region = path[0];
                std::string addressStr = path[1];
                std::string sizeStr = path[2];
                ARM* arm = (region == "arm7") ? (ARM*)NDS::ARM7 : (ARM*)NDS::ARM9;
                std::function<void(ARM*,u32,u32*)> func;
                if(sizeStr == "1" || sizeStr == "8" || sizeStr == "byte") {
                    func = &ARM::DataRead8;
                } else if(sizeStr == "2" || sizeStr == "16" || sizeStr == "short") {
                    func = &ARM::DataRead16;
                } else if(sizeStr == "u4" || sizeStr == "u32" || sizeStr == "uint") {
                    func = &ARM::DataRead32;
                } else if(sizeStr == "4" || sizeStr == "32" || sizeStr == "int") {
                    func = &ARM::DataRead32S;
                } else {
                    return response{
                        .responseCode = 400
                    };
                }
                u32 val;
                int address;
                try {
                    address = std::stoi(addressStr, nullptr, 0);
                } catch(std::invalid_argument&) {
                    return response{
                        .responseCode = 400
                    };
                } catch(std::out_of_range&) {
                    return response{
                        .responseCode = 400
                    };
                }
                func(arm, address, &val);


                // struct http_response_s* response = http_response_init();
                // http_response_status(response, 200);
                // http_response_header(response, "Content-Type", "text/plain");
                // std::string body = std::to_string(val);
                // http_response_header(response, "Content-Length", std::to_string(body.length()).c_str());
                // http_response_body(response, body.c_str(), body.length());
                // http_respond(request, response);
                return response{
                    .headers = std::map<std::string, std::string>{
                        {"Content-Type", "text/plain"}
                    },
                    .body = std::to_string(val)
                };
            } else {
                return response{
                    .responseCode = 400
                };
            }
        });
    }).detach();
}