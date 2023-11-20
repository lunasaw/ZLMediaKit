
#include "IPAddress.h"
#include "Util/logger.h"
#include <regex>
#include <sstream>
#include <algorithm>

namespace mediakit {
IPAddress::IPAddress() {

}

IPAddress::~IPAddress() {

}

std::string IPAddress::getIP(const std::string &url) {
    std::regex ipRegex(R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))");
    std::smatch match;

     if (std::regex_search(url, match, ipRegex)) {
        std::string ip = match[1].str();
        return ip;
     }
     return "";
}

bool IPAddress::isIPReachable(const std::string &ip) {
    if (!ip.empty()) {
        std::string command = "ping -c 1 " + ip;
        int result = std::system(command.c_str());
        InfoL << "ping:" << command << ",result:" << !result;
        // 根据返回值判断是否可联通
        if (result == 0) {
            return true;
        } else {
            return false;
        }
    }
    return false;
}

}