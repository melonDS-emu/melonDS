#pragma once
#include<string>
#include<map>
#include "../types.h"
class request
{
    public:
        std::string method;
        std::string path;
        std::string httpVersion;
        std::map<std::string,std::string> headers;
        std::string body;
};
class response
{
    public:
        u16 responseCode = 200;
        std::map<std::string,std::string> headers;
        std::string body = "";
};
void http(int port, response (*handler)(const request));