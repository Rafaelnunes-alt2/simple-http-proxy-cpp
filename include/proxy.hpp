// feito por: maquinzz
#pragma once
#include <cstdint>
#include <string>
namespace hproxy {
constexpr std::size_t kMaxHead = 64 * 1024;
constexpr std::size_t kMaxBody = 1024 * 1024;
constexpr int kMaxConns = 128;
struct Target {
    std::string host;
    int port = 80;
};
bool parseTarget(const std::string &s, Target &out);
bool parseHostHeader(const std::string &head, std::string &host, int &port);
std::string buildForward(const std::string &head);
}