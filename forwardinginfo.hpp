#ifndef DM_FORWARDINGINFO_HPP
#define DM_FORWARDINGINFO_HPP
#include <iostream>
#include <list>
#include <map>
#include <string>
namespace dm {

struct clientInfo
{
    std::string name;
    int port;
    struct bufferevent* bev;
};

class ForwardingInfo
{
public:
    std::string serverName_;
    int serverPort_;

    std::string clientName_;
    int clientPort_;

    std::map<int, struct clientInfo> clients_;
    std::list<int> sockets_;

    ForwardingInfo();
    ForwardingInfo(std::string serverName, int serverPort);

    friend std::ostream& operator<<(std::ostream& os, const ForwardingInfo& fi);
};

} // namespace dm
#endif
