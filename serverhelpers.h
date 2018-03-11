#include "serverstructs.h"
using namespace std;


/*-----------------------------------------------------------------------------
 * Method: cmp_sockaddr(const struct sockaddr_in &a, const struct sockaddr_in &b)
 * Used for comparison of sockaddr_in
 *---------------------------------------------------------------------------*/
bool cmp_sockaddr(const struct sockaddr_in &a, const struct sockaddr_in &b) {
    return (a.sin_port == b.sin_port && a.sin_addr.s_addr == b.sin_addr.s_addr);
}
/*-----------------------------------------------------------------------------
 * Method: genRandom(..)
 * Reads random bytes from /dev/urandom
 *---------------------------------------------------------------------------*/
uint64_t genRandom (){
    ifstream random("/dev/urandom", ios_base::in);
    uint64_t t;
    random.read(reinterpret_cast<char*>(&t), sizeof(t));
    //cout << t << endl;
    random.close();
    return t;
}  /* -- genRandom -- */
/*-----------------------------------------------------------------------------
 * Method: hostname to IP address
 *---------------------------------------------------------------------------*/
int hostname_to_ip(char *hostname , char *ip)
{
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_in *h;
    int rv;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if ( (rv = getaddrinfo( hostname , "http" , &hints , &servinfo)) != 0)
    {
        cout<<"getaddrinfo: "<<gai_strerror(rv)<<"\n";
        return 1;
    }
    
    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        h = (struct sockaddr_in *) p->ai_addr;
        strcpy(ip , inet_ntoa( h->sin_addr ) );
    }
    
    freeaddrinfo(servinfo);
    return 0;
}  /* -- hostname_to_ip -- */
/*-----------------------------------------------------------------------------
 * Method: addServer(..)
 * makes table from server information provided by command line
 *---------------------------------------------------------------------------*/
void addServer(char *argV[], sockaddr_in serverTable[], int i) {
    //Extract server IP
    //char *hostname = argV[i*2 + 3];
    char serverIp[20];
    hostname_to_ip(argV[i*2 + 3] , serverIp);
    //cout<<hostname<<" resolved to "<<serverIp;
    //fflush( stdout );
    //Extract server port
    int sport = atoi(argV[i*2 + 4]);
    if(sport == 0){
        cout<<"invalid port..\n";
        exit(1);
    }
    sockaddr_in cServer;
    memset((char *) &cServer, 0, sizeof(cServer));
    cServer.sin_family = AF_INET;
    cServer.sin_port = htons(sport);
    if (inet_aton(serverIp, &cServer.sin_addr)==0) {
        cout<<"inet_aton() failed\n";
        exit(1);
    }
    serverTable[i] = cServer;
}   /* -- addServer -- */

/*-----------------------------------------------------------------------------
 * Method: createTimer(..)
 *---------------------------------------------------------------------------*/
void createTimer(int& timer_join , itimerspec& it, int interval) {
    timespec now;
    
    if (clock_gettime(CLOCK_REALTIME, &now) == -1)
        cout<<"clock_gettime";
    
    it.it_value.tv_sec=now.tv_sec + interval;
    it.it_value.tv_nsec=now.tv_nsec;
    it.it_interval.tv_sec=0;
    it.it_interval.tv_nsec=0;
    
    timer_join=timerfd_create(CLOCK_REALTIME, 0);
    if (timer_join == -1)
        cout<<"timerfd_create";
    if (timerfd_settime(timer_join, TFD_TIMER_ABSTIME, &it, NULL) == -1)
        cout<<"timerfd_settime";
}  /* -- createTimer -- */

/*-----------------------------------------------------------------------------
 * Method: setTimer(..)
 *---------------------------------------------------------------------------*/
void setTimer(int& timer_join , itimerspec& it, int interval) {
    timespec now;
    
    if (clock_gettime(CLOCK_REALTIME, &now) == -1)
        cout<<"clock_gettime";
    
    it.it_value.tv_sec=now.tv_sec + interval;
    it.it_value.tv_nsec=now.tv_nsec;
    it.it_interval.tv_sec=0;
    it.it_interval.tv_nsec=0;
    
    if (timerfd_settime(timer_join, TFD_TIMER_ABSTIME, &it, NULL) == -1)
        cout<<"timerfd_settime";
}  /* -- setTimer -- */

/*-----------------------------------------------------------------------------
 * Method: usage(..)
 * Scope: Server
 *---------------------------------------------------------------------------*/
void usage(char* argv0)
{
    //cout<<"DuckChat Server\n";
    cout<<"Usage: "<< argv0 << " domain_name port_num\n";
} /* -- usage -- */

/*-----------------------------------------------------------------------------
 * Method: receiveType
 *---------------------------------------------------------------------------*/
int receiveType(char* buf){
    request* r = (struct request*)buf;
    return ((*r).req_type);
}

/*-----------------------------------------------------------------------------
 * Method: printChannels
 *---------------------------------------------------------------------------*/
void printChannels(channelMap *chTable){
    cout<<"\n**** Printing Channels: Total Channels = "<<chTable->size()<<" ****";
    for (channelMap::iterator it=chTable->begin(); it != chTable->end(); ++it){
        cout <<"\n"<<it->first<<"-"<<(it->second).size()<<" Users";
        for (set<sockaddr_in, compareUser>::iterator itU=(it->second).begin(); itU != (it->second).end(); ++itU)
            cout<<"\n"<<inet_ntoa(itU->sin_addr) << ":" << ntohs(itU->sin_port);
    }
    fflush( stdout );
}
/* -- printChannels -- */
/*-----------------------------------------------------------------------------
 * Method: printUsers
 *---------------------------------------------------------------------------*/
void printUsers(users *userTable){
    cout<<"\n**** Printing Users: Table Size = "<<userTable->size()<<" ****";
    for (users::iterator it=userTable->begin(); it != userTable->end(); ++it){
        cout <<"\nUser "<<(it->second).userName<<" from "<<inet_ntoa((it->first).sin_addr) << ":" << ntohs((it->first).sin_port);
        fflush( stdout );
        tm* now = localtime( & ((it->second).lastSent) );
        cout<<" Last Sent: "<<(now->tm_mon + 1) << '/'<<  now->tm_mday<<'/'<<(now->tm_year + 1900)<<"  ";
        fflush( stdout );
        cout<<now->tm_hour<<":"<<now->tm_min<<":"<<now->tm_sec;
        fflush( stdout );
    }
}/* -- printUsers -- */

/*-----------------------------------------------------------------------------
 * Method: printServers
 *---------------------------------------------------------------------------*/
void printServers(subscribedTable *subTable){
    cout<<"\n**** Printing Servers: Total Channels = "<<subTable->size()<<" ****";
    for (subscribedTable::iterator it=subTable->begin(); it != subTable->end(); ++it){
        cout <<"\n"<<it->first<<"-"<<(it->second).size()<<" Servers";
        fflush( stdout );
        for (serverSet::iterator itU=(it->second).begin(); itU != (it->second).end(); ++itU){
            cout<<"\n";/*<<inet_ntoa((itU->ipInfo).sin_addr) << ":" */cout<< ntohs((itU->ipInfo).sin_port)<<"  ";
            tm* now = localtime( & (itU->lastSent) );
        cout<<" Last Sent: "<<(now->tm_mon + 1) << '/'<<  now->tm_mday<<'/'<<(now->tm_year + 1900)<<"  ";
        cout<<now->tm_hour<<":"<<now->tm_min<<":"<<now->tm_sec;
            fflush( stdout );
        }
    }
}
/* -- printChannels -- */