#include "forwardinginfo.hpp"
namespace dm {

ForwardingInfo::ForwardingInfo() 
{ }
    
ForwardingInfo::ForwardingInfo(std::string serverName, int serverPort)
    : serverName_(serverName), serverPort_(serverPort) 
{ }

std::ostream&	
operator<<(std::ostream& os, const ForwardingInfo& fi)
{
    return os << "Server Name:    " << fi.serverName_ << "\n"
              << "Server Port:    " << fi.serverPort_ << "\n";
}

} // namespace dm
