#ifdef _WIN32
#include "http.hpp"
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#else

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

// Couple compatibility hacks
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#define WSACleanup()
#define WSAStartup(x,y) 0
#define WSADATA int
#endif
#include <cstdlib>

#include "http.h"

#include <sstream>
#include <thread>
#include <vector>
using namespace katiehttp;

std::string reasonCode(const int code)
{
    switch(code) {
        case 100: return "Continue";
        case 101: return "Switching Protocols";
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 203: return "Non-Authoritative Information";
        case 204: return "No Content";
        case 205: return "Reset Content";
        case 206: return "Partial Content";
        case 300: return "Multiple Choices";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 305: return "Use Proxy";
        case 307: return "Temporary Redirect";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 402: return "Payment Required";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 406: return "Not Acceptable";
        case 407: return "Proxy Authentication Required";
        case 408: return "Request Timeout";
        case 409: return "Conflict";
        case 410: return "Gone";
        case 411: return "Length Required";
        case 412: return "Precondition Failed";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 415: return "Unsupported Media Type";
        case 416: return "Range Not Satisfiable";
        case 417: return "Expectation Failed";
        case 426: return "Upgrade Required";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        default: return "Unknown";
    }
}

std::string recvs(SOCKET s, int len)
{
    char* buf = new char[len];
    int pos = 0;
    while(pos != len) {
        pos += recv(s, buf, len - pos, 0); // TODO error check
    }
    std::string ret = std::string(buf, len);
    free(buf);
    return ret;
}
std::string recvWord(SOCKET s)
{
    std::string word;
    char lastChar;
    recv(s, &lastChar, 1, 0);
    while(lastChar != ' ') {
        word = word.append(&lastChar, 1);
        recv(s, &lastChar, 1, 0);
    }
    return word;
}
std::string recvLine(SOCKET s)
{
    std::string line;
    char lastChar;
    recv(s, &lastChar, 1, 0);
    while(lastChar != '\r') {
        line = line.append(&lastChar, 1);
        recv(s, &lastChar, 1, 0);
    }
    recv(s, &lastChar, 1, 0); // get \n
    return line;
}
void sends(SOCKET s,const std::string& data)
{
    std::size_t len = data.size();
    while(len > 0) {
        len -= send(s, data.c_str(), len, 0); // TODO error check
    }
}

query parse_query(const std::string& string) {
    query result {
        .query_string = string
    };
    std::istringstream stream(string);
    std::string token;
    while(std::getline(stream, token, '&')) {
        if(token.contains('=')) {
            const size_t equals_pos = token.find('=');
            result.parts.emplace(token.substr(0, equals_pos), token.substr(equals_pos+1));
        }
    }
    return result;
}

path parse_path(const std::string & string) {
    std::string local_path = string;
    path result {
        .path_string = local_path
    };
    if(local_path.contains('?')) {
        const size_t question_mark_pos = local_path.find('?');
        result.query = parse_query(local_path.substr(question_mark_pos+1));
        local_path = local_path.substr(0, question_mark_pos);
    } else {
        result.query = std::nullopt;
    }
    std::istringstream stream(local_path);
    std::string token;
    while (std::getline(stream, token, '/')) {
        if(!token.empty()) result.parts.push_back(token);
    }
    return result;
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (std::string::npos == first) {
        return str;
    }

    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}
// https://www.binarytides.com/code-tcp-socket-server-winsock/
void katiehttp::http(int port, response_handler handler)
{
    std::vector<std::unique_ptr<std::thread>> threads;

    WSADATA wsa;
    
 
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
    {
        return; // TODO error check
    }
    SOCKET serverSocket, clientSocket;
    sockaddr_in server{}, client{};
    //Create a socket
    if((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        return; // TODO error check
    }
 
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);
     
    //Bind
    if( bind(serverSocket, reinterpret_cast<sockaddr *>(&server) , sizeof(server)) == SOCKET_ERROR)
    {
        return; // TODO error check
    }
     
    //Listen to incoming connections
    listen(serverSocket, 3);
     
    socklen_t size = sizeof(struct sockaddr_in);
    while((clientSocket = accept(serverSocket, reinterpret_cast<sockaddr *>(&client), &size)))
    {
        if (clientSocket == INVALID_SOCKET)
        {
            continue;
        }
        threads.push_back(std::make_unique<std::thread>([clientSocket, handler] {
            request req;
            req.method = recvWord(clientSocket);
            req.path = parse_path(recvWord(clientSocket));
            req.httpVersion = recvLine(clientSocket);
            std::string headerLine = recvLine(clientSocket);
            while(!headerLine.empty()) {
                size_t colonPosition = headerLine.find(':');
                std::string key = headerLine.substr(0, colonPosition);
                std::string value = trim(headerLine.substr(colonPosition + 1));
                req.headers.emplace(key, value);
                headerLine = recvLine(clientSocket);
            }
            auto contentLengthPosition = req.headers.find("Content-Length");
            int bodyLength = contentLengthPosition == req.headers.end() ? 0 : std::stoi(contentLengthPosition->second);
            std::string body = recvs(clientSocket, bodyLength);
            response res = handler(&req);
            res.headers.try_emplace("Content-Length", std::to_string(res.body.length()));
            sends(clientSocket, req.httpVersion + " " + std::to_string(res.responseCode) + " " + reasonCode(res.responseCode) + "\r\n");
            for(auto &[key, value] : res.headers) {
                sends(clientSocket, key);
                sends(clientSocket, ": ");
                sends(clientSocket, value);
                sends(clientSocket, "\r\n");
            }
            sends(clientSocket, "\r\n");
            sends(clientSocket, res.body);
            //Reply to client
            closesocket(clientSocket);
        }));
    }
    for (auto& thread : threads) thread->join();

    WSACleanup();
}

// int main(){
//     http(8080, [](const request req){
//         return response{
//             .responseCode = 200,
//             .body = "Hello world",
//         };
//     });
//     return 0;
// }