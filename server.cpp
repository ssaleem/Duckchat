#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <algorithm>    // std::max
#include <sys/timerfd.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <set>
#include <map>

#include "serverutils.h"



int main(int argc , char *argv[])
{
    if(argc != 3)
    {
        usage(argv[0]);
        exit(1);
    }
    
    //Extract server IP
    char *hostname = argv[1];
    char serverIp[20];
    hostname_to_ip(hostname , serverIp);
    //cout<<hostname<<" resolved to "<<serverIp;
    fflush( stdout );
    //Extract server port
    int sport = atoi(argv[2]);
    if(sport == 0){
        cout<<"invalid port..\n";
        exit(1);
    }
    sockaddr_in cServer, si_other;
    int s, slen=sizeof(si_other);
    char buf[BUFLEN];
    
    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
        diep("socket");
    
    memset((char *) &cServer, 0, sizeof(cServer));
    cServer.sin_family = AF_INET;
    cServer.sin_port = htons(sport);
    if (inet_aton(serverIp, &cServer.sin_addr)==0) {
        cout<<"inet_aton() failed\n";
        exit(1);
    }
    if (bind(s, (struct sockaddr *)&cServer, sizeof(cServer))==-1)
        diep("bind() failed");
    
    //Add channel table
    map <string, set<sockaddr_in, compareUser> > channelTable;
    //Add user table
    map<sockaddr_in, user, compareUser> userTable;
    //reading from socket and timer
    fd_set readfds;
    FD_ZERO(&readfds);
    int activity;
    
    int timer_fd;
    struct itimerspec its;
    struct timespec now;
    
    if (clock_gettime(CLOCK_REALTIME, &now) == -1)
        cout<<"clock_gettime";
    
    its.it_value.tv_sec=now.tv_sec + CLEANUP;
    its.it_value.tv_nsec=now.tv_nsec;
    its.it_interval.tv_sec=0;
    its.it_interval.tv_nsec=0;
    
    timer_fd=timerfd_create(CLOCK_REALTIME, 0);
    if (timer_fd == -1)
        cout<<"timerfd_create";
    if (timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &its, NULL) == -1)
        cout<<"timerfd_settime";
    while(1) {
        int smax = max(s, timer_fd);
        FD_CLR(s, &readfds);
        FD_CLR(timer_fd, &readfds);
        FD_SET(s, &readfds);	//Add udp socket descriptor to an fd_set
        FD_SET(timer_fd, &readfds);	//Add timerfd descriptor to an fd_set
        activity = select( smax + 1 , &readfds , NULL , NULL , NULL);
        if(activity){
            if (FD_ISSET(s, &readfds)){
                if (recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *)&si_other, (socklen_t*)&slen)==-1)
                    diep("recvfrom()");
                //cout<<"\nReceived packet from "<<inet_ntoa(si_other.sin_addr)<<":"<<ntohs(si_other.sin_port);
                //fflush( stdout );
                int type = receiveType(buf);
                if(type == REQ_LOGIN || type == REQ_LOGOUT){
                    switch (type) {
                        case REQ_LOGIN:
                            processLogin( buf, s, si_other, &userTable);
                            break;
                        case REQ_LOGOUT:
                            processLogout(si_other, &channelTable, &userTable);
                            //printUsers(&userTable);
                            //printChannels(&channelTable);
                            break;
                    }
                }
                else if(userTable.find(si_other) == userTable.end()){   //user not in user table, send error message
                    text_error t3;
                    t3.txt_type = TXT_ERROR;
                    char e[SAY_MAX] = "Not logged in";
                    strcpy(t3.txt_error, e);
                    if (sendto(s, &t3, sizeof(t3), 0, (struct sockaddr *)&si_other, sizeof(si_other))==-1)
                        diep("Not in table-sendto()");
                }
                else {
                    switch (type) {
                        case REQ_JOIN:
                            processJoin( buf, si_other, &channelTable, &userTable);
                            break;
                        case REQ_LEAVE:
                            processLeave(buf, s, si_other, &channelTable, &userTable);
                            break;
                        case REQ_SAY:
                            processSay(buf, s, si_other, &channelTable, &userTable);
                            break;
                        case REQ_LIST:
                            processList(s, si_other, &channelTable, &userTable);
                            break;
                        case REQ_WHO:
                            processWho(buf, s, si_other, &channelTable, &userTable);
                            break;
                        case REQ_KEEP_ALIVE:
                            processAlive(si_other, &userTable);
                            break;
                        default:
                            cout<<"invalid message\n";
                    }
                }
            }
            else if(FD_ISSET(timer_fd, &readfds)){
                //cout<<"\ntimer expired";
                fflush( stdout );
                its.it_value.tv_sec=0;  //disarm timer
                its.it_value.tv_nsec=0; //disarm timer
                if (timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &its, NULL) == -1)
                    cout<<"timerfd_resettime";
                cleanup(&channelTable, &userTable);
                //reset timer
                if (clock_gettime(CLOCK_REALTIME, &now) == -1)
                    cout<<"clock_gettime";
                
                its.it_value.tv_sec=now.tv_sec + CLEANUP;
                its.it_value.tv_nsec=now.tv_nsec;
                its.it_interval.tv_sec=0;
                its.it_interval.tv_nsec=0;
                
                if (timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &its, NULL) == -1)
                    cout<<"timerfd_settime";
            }
        }
    }
    
    close(s);
    return 0;
}