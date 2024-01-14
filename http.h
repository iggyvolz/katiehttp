#pragma once
#include <cstdint>
#include <string>
#include <map>
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
        uint16_t responseCode = 200;
        std::map<std::string,std::string> headers;
        std::string body;
};
void http(int port, response (*handler)(const request));