#ifndef DM_FORWARDINGINFO_HPP
#define DM_FORWARDINGINFO_HPP
#include <iostream>
#include <list>
#include <map>
#include <string>
namespace dm {

class ForwardingInfo
{
public:
    std::string serverName_;
    int serverPort_;

    ForwardingInfo();
    ForwardingInfo(std::string serverName, int serverPort);

    friend std::ostream& operator<<(std::ostream& os, const ForwardingInfo& fi);
};

} // namespace dm
#endif
