// feito por: maquinzz
#include "proxy.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET sock_t;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
#endif
namespace {
std::atomic<int> gConns{0};
void closeSock(sock_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    ::close(s);
#endif
}
bool sendAll(sock_t s, const std::string &m) {
    std::size_t sent = 0;
    while (sent < m.size()) {
#ifdef _WIN32
        int r = send(s, m.data() + sent, static_cast<int>(m.size() - sent), 0);
#else
        ssize_t r = send(s, m.data() + sent, m.size() - sent, 0);
#endif
        if (r <= 0) return false;
        sent += static_cast<std::size_t>(r);
    }
    return true;
}
sock_t connectUp(const std::string &host, int port) {
    sockaddr_in ad;
    std::memset(&ad, 0, sizeof(ad));
    ad.sin_family = AF_INET;
    ad.sin_port = htons(static_cast<std::uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &ad.sin_addr) != 1) return -1;
    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    if (connect(s, reinterpret_cast<sockaddr*>(&ad), sizeof(ad)) != 0) { closeSock(s); return -1; }
    return s;
}
void serve(sock_t c) {
    struct Guard { sock_t s_; ~Guard() { closeSock(s_); --gConns; } } g{c};
#ifdef _WIN32
    DWORD tv = 15000;
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv; tv.tv_sec = 15; tv.tv_usec = 0;
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    std::string head;
    char buf[8192] = {};
    while (head.find("\r\n\r\n") == std::string::npos) {
#ifdef _WIN32
        int r = recv(c, buf, sizeof(buf), 0);
#else
        ssize_t r = recv(c, buf, sizeof(buf), 0);
#endif
        if (r <= 0) return;
        head.append(buf, static_cast<std::size_t>(r));
        if (head.size() > hproxy::kMaxHead) return;
    }
    std::string host; int port = 80;
    if (!hproxy::parseHostHeader(head, host, port)) { sendAll(c, "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n"); return; }
    sock_t up = connectUp(host, port);
    if (up < 0) { sendAll(c, "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n"); return; }
    sendAll(up, hproxy::buildForward(head));
    std::size_t total = 0;
    bool upOpen = true;
    while (upOpen) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(c, &rfds);
        if (upOpen) FD_SET(up, &rfds);
        int nf = (upOpen && up > c) ? up + 1 : c + 1;
#ifdef _WIN32
        timeval tv{5, 0};
        int r = select(nf, &rfds, nullptr, nullptr, &tv);
#else
        timeval tv{5, 0};
        int r = select(nf, &rfds, nullptr, nullptr, &tv);
#endif
        if (r <= 0) break;
        if (FD_ISSET(c, &rfds)) {
#ifdef _WIN32
            int n = recv(c, buf, sizeof(buf), 0);
#else
            ssize_t n = recv(c, buf, sizeof(buf), 0);
#endif
            if (n <= 0) break;
            total += static_cast<std::size_t>(n);
            if (total > hproxy::kMaxBody) break;
            if (!sendAll(up, std::string(buf, static_cast<std::size_t>(n)))) break;
        }
        if (upOpen && FD_ISSET(up, &rfds)) {
#ifdef _WIN32
            int n = recv(up, buf, sizeof(buf), 0);
#else
            ssize_t n = recv(up, buf, sizeof(buf), 0);
#endif
            if (n <= 0) { upOpen = false; break; }
            if (!sendAll(c, std::string(buf, static_cast<std::size_t>(n)))) break;
        }
    }
    closeSock(up);
}
}
int main(int argc, char **argv) {
    if (argc != 3) { std::cerr << "usage: proxy <listenPort> <host:port>\n"; return 1; }
    int lport = atoi(argv[1]);
    hproxy::Target t;
    if (!hproxy::parseTarget(argv[2], t)) { std::cerr << "bad target\n"; return 1; }
    if (lport < 1 || lport > 65535) { std::cerr << "bad port\n"; return 1; }
#ifdef _WIN32
    WSADATA wd; if (WSAStartup(MAKEWORD(2,2), &wd) != 0) return 1;
#endif
    sock_t ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls < 0) return 1;
    int one = 1;
#ifdef _WIN32
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&one), sizeof(one));
#else
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
#endif
    sockaddr_in ad; std::memset(&ad, 0, sizeof(ad));
    ad.sin_family = AF_INET;
    ad.sin_addr.s_addr = htonl(INADDR_ANY);
    ad.sin_port = htons(static_cast<std::uint16_t>(lport));
    if (bind(ls, reinterpret_cast<sockaddr*>(&ad), sizeof(ad)) != 0) { closeSock(ls); return 1; }
    if (listen(ls, 64) != 0) { closeSock(ls); return 1; }
    std::cout << "proxy listening " << lport << " -> " << t.host << ":" << t.port << "\n";
    while (true) {
        sock_t c = accept(ls, nullptr, nullptr);
        if (c < 0) continue;
        if (gConns.load() >= hproxy::kMaxConns) { sendAll(c, "HTTP/1.1 503 Busy\r\nContent-Length: 0\r\n\r\n"); closeSock(c); continue; }
        ++gConns;
        std::thread(serve, c).detach();
    }
    return 0;
}