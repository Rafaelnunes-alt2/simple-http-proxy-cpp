// feito por: maquinzz
#include "proxy.hpp"
namespace hproxy {
namespace {
std::string lower(std::string s) {
    for (auto &c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}
}
bool parseTarget(const std::string &s, Target &out) {
    std::size_t colon = s.rfind(':');
    if (colon == std::string::npos) { out.host = s; out.port = 80; return !s.empty() && s.size() < 254; }
    std::string h = s.substr(0, colon);
    std::string p = s.substr(colon + 1);
    if (h.empty() || p.empty()) return false;
    int pv = 0;
    for (char c : p) {
        if (c < '0' || c > '9') return false;
        pv = pv * 10 + (c - '0');
        if (pv > 65535) return false;
    }
    out.host = h;
    out.port = pv;
    return h.size() < 254 && pv > 0;
}
bool parseHostHeader(const std::string &head, std::string &host, int &port) {
    std::size_t pos = head.find("\r\n");
    std::size_t start = (pos == std::string::npos) ? head.size() : pos + 2;
    while (start < head.size()) {
        std::size_t e = head.find("\r\n", start);
        if (e == std::string::npos) e = head.size();
        std::string line = head.substr(start, e - start);
        std::size_t c = line.find(':');
        if (c != std::string::npos && lower(line.substr(0, c)) == "host") {
            std::string v = line.substr(c + 1);
            while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.erase(v.begin());
            Target t;
            if (!parseTarget(v, t)) return false;
            host = t.host;
            port = t.port;
            return true;
        }
        if (e == head.size()) break;
        start = e + 2;
    }
    return false;
}
std::string buildForward(const std::string &head) {
    std::string out;
    std::size_t pos = head.find("\r\n\r\n");
    std::string hs = (pos == std::string::npos) ? head : head.substr(0, pos + 2);
    std::size_t start = 0;
    bool first = true;
    while (start < hs.size()) {
        std::size_t e = hs.find("\r\n", start);
        if (e == std::string::npos) break;
        std::string line = hs.substr(start, e - start);
        start = e + 2;
        if (first) { out += line + "\r\n"; first = false; continue; }
        std::size_t c = line.find(':');
        if (c != std::string::npos) {
            std::string k = lower(line.substr(0, c));
            if (k == "proxy-connection" || k == "connection" || k == "keep-alive") continue;
        }
        out += line + "\r\n";
    }
    out += "\r\n";
    return out;
}
}