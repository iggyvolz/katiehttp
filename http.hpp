#pragma once
#include <cstdint>
#include <string>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace katiehttp {
    struct query {
        std::string query_string;
        std::unordered_multimap<std::string, std::string> parts;
    };
    struct path {
        std::string path_string;
        std::vector<std::string> parts;
        std::optional<query> query;
    };
    struct request
    {
        std::string method;
        path path;
        std::string httpVersion;
        std::unordered_multimap<std::string,std::string> headers;
        std::string body;
    };
    struct response
    {
        uint16_t responseCode = 200;
        std::map<std::string,std::string> headers;
        std::string body;
    };
    typedef response (*response_handler)(const request*);
    void http(int port, response_handler);
}
