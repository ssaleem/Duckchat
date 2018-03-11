#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <time.h>
#include <iostream>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

using namespace std;

/*-----------------------------------------------------------------------------
 * Single user entry in user table
 *---------------------------------------------------------------------------*/
struct user{
    char userName[USERNAME_MAX];  //name
    struct sockaddr_in ipInfo;   //IP and port info to distinguish between users
    char channelName[CHANNEL_MAX];  //current active channel name
    time_t lastSent;    //for cleanup purposes
};

struct compareUser{  //string based comparison after appending port to ip, needed for map STL
    bool operator()(const struct sockaddr_in &a, const struct sockaddr_in &b) const{
        string ipport_a = inet_ntoa(a.sin_addr);
        stringstream ss;
        ss << (ntohs(a.sin_port));
        string str = ss.str();
        ipport_a.append(str);
        
        string ipport_b = inet_ntoa(b.sin_addr);
        stringstream ssb;
        ssb << (ntohs(b.sin_port));
        str = ssb.str();
        ipport_b.append(str);
        
        if(ipport_a > ipport_b)
            return true;
        return false;
    }
};

/*-----------------------------------------------------------------------------
 * Single server entry in subscribed server table
 *---------------------------------------------------------------------------*/
struct subServer{
    struct sockaddr_in ipInfo;   //IP and port info to distinguish between users
    time_t lastSent;    //for cleanup purposes in case no join
};

struct compareServer{  //string based comparison after appending port to ip, needed for set STL
    bool operator()(const struct subServer &a, const struct subServer &b) const{
        string ipport_a = inet_ntoa(a.ipInfo.sin_addr);
        stringstream ss;
        ss << (ntohs(a.ipInfo.sin_port));
        string str = ss.str();
        ipport_a.append(str);
        
        string ipport_b = inet_ntoa(b.ipInfo.sin_addr);
        stringstream ssb;
        ssb << (ntohs(b.ipInfo.sin_port));
        str = ssb.str();
        ipport_b.append(str);
        
        if(ipport_a > ipport_b)
            return true;
        return false;
    }
};
//User Table at server
typedef map<sockaddr_in, user, compareUser> users;
//Channel table at server
typedef set<sockaddr_in, compareUser> sockSet;
typedef map <string, sockSet> channelMap;
//Subscribed server table at server
typedef set<subServer, compareServer> serverSet;
typedef map <string, serverSet> subscribedTable;
//Message IDs to detect loops
typedef set<s_id> idSet;