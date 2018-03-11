#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>       //required for perror
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <algorithm>    // std::max
#include <sys/timerfd.h>
#include <sys/time.h>
#include <time.h>

#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <iostream>
#include <set>
#include "clientutils.h"
#include "raw.h"

using namespace std;

/*-----------------------------------------------------------------------------
 * Method: processList
 *---------------------------------------------------------------------------*/
void processList(int s, struct sockaddr_in cServer)
{
    request_list m5;
    memset(&m5, 0, sizeof(m5)) ;
    m5.req_type = REQ_LIST;
    if (sendto(s, &m5, sizeof(m5), 0, (struct sockaddr *)&cServer, sizeof(cServer))==-1)
        diep("processList-sendto()");
}	/* -- processList -- */

/*-----------------------------------------------------------------------------
 * Method: processExit
 *---------------------------------------------------------------------------*/
void processExit(int s, struct sockaddr_in cServer)
{
    request_logout m1;
    memset(&m1, 0, sizeof(m1)) ;
    m1.req_type = REQ_LOGOUT;
    if (sendto(s, &m1, sizeof(m1), 0, (struct sockaddr *)&cServer, sizeof(cServer))==-1)
        diep("processExit-sendto()");
}	/* -- processExit -- */

/*-----------------------------------------------------------------------------
 * Method: processJoin
 *---------------------------------------------------------------------------*/
void processJoin(char* channel, set<string> *chSet, char* curChannel, int s, struct sockaddr_in cServer )
{
    //cout<<"within processJoin()\n";  //ignore if joining an already joined channel
    std::set<string>::iterator it;
    it = (*chSet).find(channel);
    if(it == (*chSet).end()){
        (*chSet).insert(channel);
        strcpy(curChannel, channel);
        request_join m2;
        memset(&m2, 0, sizeof(m2)) ;
        m2.req_type = REQ_JOIN;
        stpcpy(m2.req_channel, channel);
        if (sendto(s, &m2, sizeof(m2), 0, (struct sockaddr *)&cServer, sizeof(cServer))==-1)
            diep("processJoin-sendto()");
    }
}	/* -- processJoin -- */

/*-----------------------------------------------------------------------------
 * Method: processLeave
 *---------------------------------------------------------------------------*/
void processLeave(char* channel, set<string> *chSet, int s, struct sockaddr_in cServer)
{
    //cout<<"within processLeave()\n";
    std::set<string>::iterator it;
    it = (*chSet).find(channel);
    if(it != (*chSet).end()){   //remove channel from the local list at client only if its already in there
        (*chSet).erase(channel);
    }
    //message is sent anyway...if wrong request server will reply with error
    request_leave m3;
    memset(&m3, 0, sizeof(m3)) ;
    m3.req_type = REQ_LEAVE;
    stpcpy(m3.req_channel, channel);
    if (sendto(s, &m3, sizeof(m3), 0, (struct sockaddr *)&cServer, sizeof(cServer))==-1)
        diep("processLeave-sendto()");
}	/* -- processLeave -- */

/*-----------------------------------------------------------------------------
 * Method: processWho
 *---------------------------------------------------------------------------*/
void processWho(char* channel, int s, struct sockaddr_in cServer)
{
    //cout<<"within processWho()\n";
    request_who m6;
    memset(&m6, 0, sizeof(m6)) ;
    m6.req_type = REQ_WHO;
    stpcpy(m6.req_channel, channel);
    if (sendto(s, &m6, sizeof(m6), 0, (struct sockaddr *)&cServer, sizeof(cServer))==-1)
        diep("processWho-sendto()");
}	/* -- processWho -- */


/*-----------------------------------------------------------------------------
 * Method: processCommand
 *---------------------------------------------------------------------------*/
int processCommand(char* command, set<string> *chSet, char* curChannel, int s, struct sockaddr_in cServer, int& resettimer)
{
    int closeClient = 1;
    int commandType;
    char Tokens[2][CHANNEL_MAX];
    if(strlen(command) > CHANNEL_MAX + 10){
        cout<<"\n*Unknown command";
        return closeClient;
    }
    int commandToks = tokCommand(command, Tokens) ;
    switch (commandToks)
    {
        case 1:        //only commands
            //cout<<"Command: "<<Tokens[0]<<"\n";
            commandType = checkCommandType(Tokens[0]);
            switch (commandType){
                case LIST:
                    processList(s, cServer);
                    resettimer = 1;
                    break;
                case EXIT:
                    processExit(s, cServer);
                    closeClient = 0;
                    break;
                default:
                    cout<<"\n*Unknown command";
            }
            break;
        case 2:         //commands with args
            //cout<<"Command: "<<Tokens[0]<<"\n";
            //cout<<"Channel: "<<Tokens[1]<<"\n";
            commandType = checkCommandType(Tokens[0]);
            switch (commandType){
                case JOIN:
                    processJoin(Tokens[1], chSet, curChannel, s, cServer);
                    resettimer = 1;
                    break;
                case LEAVE:
                    processLeave(Tokens[1], chSet, s, cServer);
                    resettimer = 1;
                    break;
                case WHO:
                    processWho(Tokens[1], s, cServer);
                    resettimer = 1;
                    break;
                case SWITCH:
                    processSwitch(Tokens[1], chSet, curChannel);
                    break;
                default:
                    cout<<"\n*Unknown command";
            }
            break;
        default:
            cout<<"*Unknown command\n";
    }
    return closeClient;
}	/* -- processCommand -- */



int main(int argc , char *argv[])
{
    //--------------Extract commnad line arguments------------------------TODO: type checking of input arguments
    if(argc != 4)
    {
        usage(argv[0]);
        exit(1);
    }
    //Extract server IP
    char *hostname = argv[1];
    char serverIp[20];
    hostname_to_ip(hostname , serverIp);
    //cout<<hostname<<" resolved to "<<serverIp;
    
    //Extract server port
    int sport = atoi(argv[2]);
    if(sport == 0){
        cout<<"invalid port..\n";
        exit(1);
    }
    raw_mode();
    //reading from socket and cin
    fd_set readfds;
    FD_ZERO(&readfds);
    int activity;
    
    char c = 'a';
    
    //--------------Allocate & Initialize Variables in Client------------------------
    char userInput[SAY_MAX];
    char my_channel[CHANNEL_MAX]; //stores currently active channe
    strcpy(my_channel, COMMON);
    set<string> subscribedChannels;   //stores channel list
    subscribedChannels.insert(COMMON);
    char my_username[USERNAME_MAX];  //stores user name
    if(strlen(argv[3]) > USERNAME_MAX)
    {
        cout<<"error resolving hostname..\n";
        exit(1);
    }
    strcpy(my_username, argv[3]);
    
    //---------------Create Socket and check if server address is valid---------------
    struct sockaddr_in cServer;
    int s;
    int slen = sizeof(cServer);
    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
        diep("socket");
    
    memset((char *) &cServer, 0, sizeof(cServer));
    cServer.sin_family = AF_INET;
    cServer.sin_port = htons(sport);
    if (inet_aton(serverIp, &cServer.sin_addr)==0) {
        cout<<"inet_aton() failed\n";
        exit(1);
    }
    
    //------------------Structs for different messages (client to server)-----------------------
    request_login m0;
    request_join m2;
    request_say m4;
    
    //------------------Structs for different messages (server to client)-----------------------
    char buf[BUFLEN];
    
    //-----------------Send initial login and join---------------------------
    //login
    memset(&m0, 0, sizeof(m0)) ;
    m0.req_type = REQ_LOGIN;
    stpcpy(m0.req_username, my_username);
    if (sendto(s, &m0, sizeof(m0), 0, (struct sockaddr *)&cServer, slen)==-1)
        diep("Login-sendto()");
    //join
    memset(&m2, 0, sizeof(m2)) ;
    m2.req_type = REQ_JOIN;
    stpcpy(m2.req_channel, my_channel);
    if (sendto(s, &m2, sizeof(m2), 0, (struct sockaddr *)&cServer, slen)==-1)
        diep("Initial Join-sendto()");
    
    //--------Set timer file descriptor, KEEPALIVE is the timer interval------
    int timer_fd;
    struct itimerspec its;
    struct timespec now;
    
    if (clock_gettime(CLOCK_REALTIME, &now) == -1)
        cout<<"clock_gettime";
    
    its.it_value.tv_sec=now.tv_sec + KEEP_ALIVE;
    its.it_value.tv_nsec=now.tv_nsec;
    its.it_interval.tv_sec=0;
    its.it_interval.tv_nsec=0;
    
    timer_fd=timerfd_create(CLOCK_REALTIME, 0);
    if (timer_fd == -1)
        cout<<"timerfd_create";
    if (timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &its, NULL) == -1)
        cout<<"timerfd_settime";
    
    //--------generte prompt, distinguish between command and message------
    int closeClient = 1;
    int resettimer = 0;
    int k;
    while(closeClient){
        k = 0;     //for input chars
        int smax = max(s, timer_fd);
        cout<<"\n>";
        fflush( stdout );
        
        while(c != '\n'){
            //Prepare file descriptors set for select()
            FD_CLR(s, &readfds);
            FD_CLR(timer_fd, &readfds);
            FD_SET(STDIN_FILENO, &readfds);  //Add cin descriptor to fd_set
            FD_SET(s, &readfds);	//Add udp socket descriptor to fd_set
            FD_SET(timer_fd, &readfds);	//Add timerfd descriptor to fd_set
            
            activity = select( smax + 1 , &readfds , NULL , NULL , NULL);
            
            if(activity){
                if (FD_ISSET(s, &readfds)){
                    if (recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *)&cServer, (socklen_t*)&slen)==-1)
                        diep("recvfrom()");
                    //cout<<"Received packet from"<<inet_ntoa(cServer.sin_addr)<<" : "<< ntohs(cServer.sin_port);
                    cout << string(k+1,'\b');
                    displayReceived(buf);
                    cout<<"\n>";
                    for(int i = 0; i < k; i++)
                        cout<<userInput[i];
                    fflush( stdout );
                    
                }
                else if(FD_ISSET(STDIN_FILENO, &readfds))
                {
                    c = getchar();
                    if (c != '\n'){
                        cout<<c;
                        fflush( stdout );
                        
                        userInput[k++] = c;
                    }
                }
                else if(FD_ISSET(timer_fd, &readfds)){
                    //cout<<"timer expired\n";
                    its.it_value.tv_sec=0;  //disarm timer
                    its.it_value.tv_nsec=0; //disarm timer
                    if (timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &its, NULL) == -1)
                        cout<<"timerfd_resettime";
                    keepAlive(s, cServer);
                    //reset timer
                    if (clock_gettime(CLOCK_REALTIME, &now) == -1)
                        cout<<"clock_gettime";
                    
                    its.it_value.tv_sec=now.tv_sec + KEEP_ALIVE;
                    its.it_value.tv_nsec=now.tv_nsec;
                    its.it_interval.tv_sec=0;
                    its.it_interval.tv_nsec=0;
                    
                    if (timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &its, NULL) == -1)
                        cout<<"timerfd_settime";
                }
            }
            else
                cout<<"Error in selection()\n";
        }
        userInput[k]='\0';
        
        //cout<<">"<<userInput<<"\n";
        char inputType = userInput[0];
        switch (inputType)
        {
            case '/' :
                resettimer = 0;
                closeClient = processCommand((userInput+1), &subscribedChannels, my_channel, s, cServer,resettimer);//+1 is to get rid of /
                if(resettimer){
                    //reset timer
                    if (clock_gettime(CLOCK_REALTIME, &now) == -1)
                        cout<<"clock_gettime";
                    
                    its.it_value.tv_sec=now.tv_sec + KEEP_ALIVE;
                    its.it_value.tv_nsec=now.tv_nsec;
                    its.it_interval.tv_sec=0;
                    its.it_interval.tv_nsec=0;
                    
                    if (timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &its, NULL) == -1)
                        cout<<"timerfd_settime";
                }
                break;
            default:
                memset(&m4, 0, sizeof(m4)) ;
                m4.req_type = REQ_SAY;
                stpcpy(m4.req_channel, my_channel);
                strcpy(m4.req_text, userInput);
                
                if (sendto(s, &m4, sizeof(m4), 0, (struct sockaddr *)&cServer, slen)==-1)
                    diep("request_say-sendto()");
                //reset timer
                if (clock_gettime(CLOCK_REALTIME, &now) == -1)
                    cout<<"clock_gettime";
                
                its.it_value.tv_sec=now.tv_sec + KEEP_ALIVE;
                its.it_value.tv_nsec=now.tv_nsec;
                its.it_interval.tv_sec=0;
                its.it_interval.tv_nsec=0;
                
                if (timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &its, NULL) == -1)
                    cout<<"timerfd_settime";
        }
        c = 'a';
    }
    close(s);
    
    //change prompt back to line input
    cooked_mode();
    
    return 0;
}