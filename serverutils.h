#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <time.h>
#include <iostream>
#include <cstring>
#include <string>
#include <sstream>
#include "duckchat.h"

using namespace std;

struct user{
    char userName[USERNAME_MAX];  //name
    struct sockaddr_in ipInfo;   //IP and port info for ID
    char channelName[CHANNEL_MAX];  //current active channel name
    time_t lastSent;
};
struct compareUser{  //string based comparison after appending port to ip
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
 * Method: usage(..)
 * Scope: local
 *---------------------------------------------------------------------------*/
void usage(char* argv0)
{
    //cout<<"DuckChat Server\n";
    cout<<"Usage: "<< argv0 << " domain_name port_num\n";
} /* -- usage -- */
void sendError(char* e, int s, sockaddr_in client){
    text_error t3;
    t3.txt_type = TXT_ERROR;
    strcpy(t3.txt_error, e);
    if (sendto(s, &t3, sizeof(t3), 0, (struct sockaddr *)&client, sizeof(client))==-1)
        diep("processLeave-sendto()");
}
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
 * Method: receiveType
 *---------------------------------------------------------------------------*/
int receiveType(char* buf){
    struct request* r = (struct request*)buf;
    return ((*r).req_type);
}

/*-----------------------------------------------------------------------------
 * Method: printChannels
 * Scope: local
 *---------------------------------------------------------------------------*/
void printChannels(map <string, set<sockaddr_in, compareUser> > *chTable){
    cout<<"\n**** Printing Channels: Table Size = "<<chTable->size()<<" ****";
    for (map <string, set<sockaddr_in, compareUser> >::iterator it=chTable->begin(); it != chTable->end(); ++it){
        cout <<"\nChannel: "<<it->first<<"-"<<(it->second).size()<<" Users\n";
        fflush( stdout );
        for (set<sockaddr_in, compareUser>::iterator itU=(it->second).begin(); itU != (it->second).end(); ++itU){
            cout<<inet_ntoa(itU->sin_addr) << ":" << ntohs(itU->sin_port)<<"\n";
            fflush( stdout );
        }
    }
}
/* -- printChannels -- */
/*-----------------------------------------------------------------------------
 * Method: printUsers
 *---------------------------------------------------------------------------*/
void printUsers(map<sockaddr_in, user, compareUser> *userTable){
    cout<<"\n**** Printing Users: Table Size = "<<userTable->size()<<" ****";
    for (map<sockaddr_in, user, compareUser>::iterator it=userTable->begin(); it != userTable->end(); ++it){
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
 * Method: processAlive
 *---------------------------------------------------------------------------*/
void processAlive(sockaddr_in client, map<sockaddr_in, user, compareUser> *userTable){
    //if(userTable->find(client) != userTable->end())    //doing this in server.cpp
    (*userTable)[client].lastSent = time(0);
    cout<<"\nserver: "<<(*userTable)[client].userName<<" keeps alive";
    fflush( stdout );
    
}/* -- processAlive -- */
/*-----------------------------------------------------------------------------
 * Method: processSay
 *---------------------------------------------------------------------------*/
void processSay(char* buf, int s, sockaddr_in client, map <string, set<sockaddr_in, compareUser> > *chTable, map<sockaddr_in, user, compareUser> *userTable ){
    request_say* r4 = (struct request_say*)buf;
    if(strlen(r4->req_text) > SAY_MAX-1){
        cout<<"\nserver: "<<(*userTable)[client].userName<<" sends **bad message in "<<r4->req_channel;
        char e[SAY_MAX]  = "**Bad message-Discarded";
        sendError(e,s,client);
        return;
    }
    cout<<"\nserver: "<<(*userTable)[client].userName<<" sends say message in "<<r4->req_channel;
    fflush( stdout );
    (*userTable)[client].lastSent = time(0);
    char chName[CHANNEL_MAX];
    strcpy(chName, r4->req_channel);
    if(strcmp(chName, " ") != 0)  { //user has some active channel
        text_say t0;
        t0.txt_type = TXT_SAY;
        strcpy(t0.txt_channel, chName);
        strcpy(t0.txt_username, (*userTable)[client]. userName);
        strcpy(t0.txt_text, r4->req_text);
        for (set<sockaddr_in, compareUser>::iterator itU=((*chTable)[chName]).begin(); itU != ((*chTable)[chName]).end(); ++itU)
            if (sendto(s, &t0, sizeof(t0), 0, (struct sockaddr *)&(*itU), sizeof(*itU))==-1)
                diep("processSay-sendto()");
    }
    else
        cout<<"\nserver: "<<(*userTable)[client].userName<<" trying to send a message to channel Common where he/she is not a member";
    
}/* -- processSay -- */
/*-----------------------------------------------------------------------------
 * Method: processLeave
 *---------------------------------------------------------------------------*/
void processLeave(char* buf, int s, sockaddr_in client, map <string, set<sockaddr_in, compareUser> > *chTable, map<sockaddr_in, user, compareUser> *userTable){
    request_leave* r3 = (request_leave*)buf;
    (*userTable)[client].lastSent = time(0);
    
    text_error t3;
    t3.txt_type = TXT_ERROR;
    
    if(chTable->find(r3->req_channel) == chTable->end()){
        cout<<"\nserver: "<<(*userTable)[client].userName<<" trying to leave non-existent channel "<<r3->req_channel;
        char e[SAY_MAX] = "No channel by  the name ";
        strcat(e,r3->req_channel);
        strcpy(t3.txt_error, e);
        if (sendto(s, &t3, sizeof(t3), 0, (struct sockaddr *)&client, sizeof(client))==-1)
            diep("processLeave-sendto()");
        return;
    }
    else if((*chTable)[r3->req_channel].find(client) == (*chTable)[r3->req_channel].end()){
        cout<<"\nserver: "<<(*userTable)[client].userName<<" trying to leave channel "<<r3->req_channel<<" where he/she is not a member";
        char e[SAY_MAX] = "You are not in channel ";
        strcat(e,r3->req_channel);
        strcpy(t3.txt_error, e);
        if (sendto(s, &t3, sizeof(t3), 0, (struct sockaddr *)&client, sizeof(client))==-1)
            diep("processLeave-sendto()");
        return;
    }
    else{
        cout<<"\nserver: "<<(*userTable)[client].userName<<" leaves channel "<<r3->req_channel;
        (*chTable)[r3->req_channel].erase(client);
        if((*chTable)[r3->req_channel].empty())   //if no users, delete channel
            chTable->erase(r3->req_channel);
        if(strcmp((*userTable)[client].channelName, r3->req_channel) == 0)
            strcpy((*userTable)[client].channelName, " ");
        
    }
}/* -- processLeave -- */
/*-----------------------------------------------------------------------------
 * Method: processList
 *---------------------------------------------------------------------------*/
void processList(int s, sockaddr_in client, map <string, set<sockaddr_in, compareUser> > *chTable, map<sockaddr_in, user, compareUser> *userTable){
    cout<<"\nserver: "<<(*userTable)[client].userName<<" lists channels";
    fflush( stdout );
    (*userTable)[client].lastSent = time(0);
    text_list_s t1;
    t1.txt_type = TXT_LIST;
    t1.txt_nchannels = chTable->size();
    size_t sendSize = sizeof(t1) + (t1.txt_nchannels * sizeof(channel_info));
    char* sendbuf = new char[sendSize];
    memcpy(sendbuf, &t1, sizeof(t1));
    channel_info* txt_channels = new channel_info[t1.txt_nchannels];
    int i = 0;
    for (map <string, set<sockaddr_in, compareUser> >::iterator it=chTable->begin(); it != chTable->end(); ++it,i++)
        strcpy(txt_channels[i].ch_channel, (it->first).c_str());
    
    memcpy((sendbuf+sizeof(t1)), txt_channels, t1.txt_nchannels * sizeof(channel_info));
    if (sendto(s, sendbuf, sendSize, 0, (struct sockaddr *)&client, sizeof(client))==-1)
        diep("processWho-sendto()");
    
    delete[] txt_channels;
    delete[] sendbuf;
    
    
}/* -- procesList -- */
/*-----------------------------------------------------------------------------
 * Method: processWho
 *---------------------------------------------------------------------------*/
void processWho(char* buf, int s, sockaddr_in client, map <string, set<sockaddr_in, compareUser> > *chTable, map<sockaddr_in, user, compareUser> *userTable ){
    request_who* r6 = (struct request_who *)buf;
    (*userTable)[client].lastSent = time(0);
    if(chTable->find(r6->req_channel) == chTable->end()){
        text_error t3;
        t3.txt_type = TXT_ERROR;
        char e[SAY_MAX] = "No channel by  the name ";
        strcat(e,r6->req_channel);
        strcpy(t3.txt_error, e);
        if (sendto(s, &t3, sizeof(t3), 0, (struct sockaddr *)&client, sizeof(client))==-1)
            diep("processWho-sendto()");
        return;
    }
    text_who_s t2;
    t2.txt_type = TXT_WHO;
    t2.txt_nusernames = (*chTable)[r6->req_channel].size();
    strcpy(t2.txt_channel,  r6->req_channel);
    cout<<"\nserver: "<<(*userTable)[client].userName<<" lists users in channel "<<t2.txt_channel;
    fflush( stdout );
    user_info* txt_users = new user_info[t2.txt_nusernames];
    
    size_t sendSize = sizeof(t2) + (t2.txt_nusernames * sizeof(user_info));
    char* sendbuf = new char[sendSize];
    memcpy(sendbuf, &t2, sizeof(t2));
    
    int i = 0;
    for (set<sockaddr_in, compareUser>::iterator itU=((*chTable)[t2.txt_channel]).begin(); itU != ((*chTable)[t2.txt_channel]).end(); ++itU, i++)
        strcpy(txt_users[i].us_username, (*userTable)[(*itU)].userName);
    
    memcpy((sendbuf+sizeof(t2)), txt_users, t2.txt_nusernames * sizeof(user_info));
    if (sendto(s, sendbuf, sendSize, 0, (struct sockaddr *)&client, sizeof(client))==-1)
        diep("processWho()");
    
    delete[] txt_users;
    delete[] sendbuf;
}/* -- procesWho -- */
/*-----------------------------------------------------------------------------
 * Method: processLogout
 *---------------------------------------------------------------------------*/
void processLogout(sockaddr_in client, map <string, set<sockaddr_in, compareUser> > *chTable, map<sockaddr_in, user, compareUser> *userTable ){
    if(userTable->find(client) != userTable->end()){
        cout<<"\nserver: "<<(*userTable)[client].userName<<" logs out";
        fflush( stdout );
        (*userTable).erase(client);
        map <string, set<sockaddr_in, compareUser> >::iterator itu=chTable->begin();
        while(itu != chTable->end()){
            if((itu->second).find(client) !=  (itu->second).end())
                (itu->second).erase(client);
            if((itu->second).empty()) {  //if no users, delete channel
                map <string, set<sockaddr_in, compareUser> >::iterator eraseitu = itu;
                ++itu;
                chTable->erase(eraseitu);
            }
            else
                ++itu;
        }
    }
}/* -- procesLogout -- */
/*-----------------------------------------------------------------------------
 * Method: processJoin
 *---------------------------------------------------------------------------*/
void processJoin(char* buf, sockaddr_in client, map <string, set<sockaddr_in, compareUser> > *chTable, map<sockaddr_in, user, compareUser> *userTable ){
    
    (*userTable)[client].lastSent = time(0);
    request_join* r1 = (struct request_join *)buf;
    cout<<"\nserver: "<<(*userTable)[client].userName<<" joins channel "<<r1->req_channel;
    fflush( stdout );
    (*chTable)[r1->req_channel].insert(client);
    strcpy((*userTable)[client].channelName, r1->req_channel);
}/* -- procesJoin -- */
/*-----------------------------------------------------------------------------
 * Method: processLogin
 *---------------------------------------------------------------------------*/
void processLogin(char* buf, int s, sockaddr_in cServer, map<sockaddr_in, user, compareUser> *userTable ){
    user u;
    request_login* r0 = (struct request_login *)buf;
    if(strlen(r0->req_username) > USERNAME_MAX-1){
        char e[SAY_MAX]  = "Bad user name";
        sendError(e,s,cServer);
        return;
    }
    strcpy(u.userName, r0->req_username);
    cout<<"\nserver: "<<u.userName<<" logs in";
    fflush( stdout );
    u.ipInfo = cServer;
    u.lastSent = time(0);
    (*userTable)[cServer] = u;
}/* -- procesLogin -- */
/*-----------------------------------------------------------------------------
 * Method: cleanup
 *---------------------------------------------------------------------------*/
void cleanup(map <string, set<sockaddr_in, compareUser> > *chTable, map<sockaddr_in, user, compareUser> *userTable){
    time_t now = time(0);
    map<sockaddr_in, user, compareUser>::iterator it=userTable->begin();
    while(it != userTable->end()){
        if(difftime(now,(it->second).lastSent) >= CLEANUP){
            map<sockaddr_in, user, compareUser>::iterator eraseit= it;
            sockaddr_in client = eraseit->first;
            ++it;
            cout<<"\nserver: forcibly removing user "<<(eraseit->second).userName;
            fflush( stdout );
            userTable->erase(eraseit);
            map <string, set<sockaddr_in, compareUser> >::iterator itu=chTable->begin();
            while(itu != chTable->end()){
                if((itu->second).find(client) !=  (itu->second).end())
                    (itu->second).erase(client);
                if((itu->second).empty()) {  //if no users, delete channel
                    map <string, set<sockaddr_in, compareUser> >::iterator eraseitu = itu;
                    ++itu;
                    chTable->erase(eraseitu);
                }
                else
                    ++itu;
            }
        }
        else
            ++it;
    }
}/* -- cleanup -- */