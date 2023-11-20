#ifndef ZLMEDIAKIT_IPADDRESS_H
#define ZLMEDIAKIT_IPADDRESS_H


#include <string>


namespace mediakit {

class IPAddress {
public:
IPAddress();
~IPAddress();

static std::string getIP(const std::string &url);

static bool isIPReachable(const std::string &ip);

};

}

#endif ZLMEDIAKIT_IPADDRESS_H