/*****************************************************
 * usage : ./server IP Port server2IP server2Port....
 ******************************************************/
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>    // std::max
#include <sys/timerfd.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <set>
#include <map>

#include "serverutils.h"



int main(int argc , char *argv[]) {
    if(argc < 3 || ((argc % 2) == 0)) {
        usage(argv[0]);
        exit(1);
    }
    
    //Extract server IP
    char *hostname = argv[1];
    char serverIp[20];
    hostname_to_ip(hostname , serverIp);
    //cout<<hostname<<" resolved to "<<serverIp;
    //fflush( stdout );
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
    
    //Add other servers to server table
    sockaddr_in serverTable[MAXSERVERS];
    int serverTableSize = (argc-3)/2;
    for(int i = 0; i < serverTableSize && i < MAXSERVERS ; i++)   //should not overflow server table
        addServer(argv,serverTable, i);
    
    //Add channel table
    channelMap channelTable;
    //Add user table
    users userTable;
    
    //Add subscribed server table
    subscribedTable subServerTable;
    //Add say identifiers
    idSet receivedIds ;
    
    //reading from socket and timer
    fd_set readfds;
    FD_ZERO(&readfds);
    int activity;
    
    int timer_cleanup, timer_join;
    struct itimerspec it_c, it_j;
    struct timespec now;
    //create server join soft state timer
    createTimer(timer_join, it_j, SJOIN);
    
    //create client cleanup timer
    if (clock_gettime(CLOCK_REALTIME, &now) == -1)
        cout<<"clock_gettime";
    
    it_c.it_value.tv_sec=now.tv_sec + CLEANUP;
    it_c.it_value.tv_nsec=now.tv_nsec;
    it_c.it_interval.tv_sec=0;
    it_c.it_interval.tv_nsec=0;
    
    timer_cleanup=timerfd_create(CLOCK_REALTIME, 0);
    if (timer_cleanup == -1)
        cout<<"timerfd_create";
    if (timerfd_settime(timer_cleanup, TFD_TIMER_ABSTIME, &it_c, NULL) == -1)
        cout<<"timerfd_settime";
        
    while(1) {
        int smax = max(s, max(timer_cleanup, timer_join));
        FD_CLR(s, &readfds);
        FD_CLR(timer_cleanup, &readfds);
        FD_CLR(timer_join, &readfds);
        FD_SET(s, &readfds);	//Add udp socket descriptor to an fd_set
        FD_SET(timer_cleanup, &readfds);	//Add timer_cleanup descriptor to an fd_set
        FD_SET(timer_join, &readfds);	//Add timer_join descriptor to an fd_set
        activity = select( smax + 1 , &readfds , NULL , NULL , NULL);
        if(activity){
            if (FD_ISSET(s, &readfds)){
                if (recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *)&si_other, (socklen_t*)&slen)==-1)
                    diep("recvfrom()");
                //cout<<"\nReceived packet from "<<inet_ntoa(si_other.sin_addr)<<":"<<ntohs(si_other.sin_port);
                int type = receiveType(buf);
                
                if(type == REQ_LOGIN || type == REQ_LOGOUT){
                    switch (type) {
                        case REQ_LOGIN:
                        	cout<<"\n"<<serverIp<<":"<<sport;
                            processLogin( buf, s, si_other, &userTable);
                            break;
                        case REQ_LOGOUT:
                            processLogout(cServer, si_other, &channelTable, &userTable);
                            break;
                    }
                }
                else if(userTable.find(si_other) == userTable.end() && type < SREQ_JOIN){  //a non-logged in user
                    //user not in user table, send error message
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
                        	cout<<"\n"<<serverIp<<":"<<sport;
                            processJoin( buf, s, si_other, serverTable, serverTableSize, &channelTable, &userTable, &subServerTable, cServer);
                            break;
                        case REQ_LEAVE:
                        	cout<<"\n"<<serverIp<<":"<<sport;
                            processLeave(buf, s, si_other, &channelTable, &userTable);
                            break;
                        case REQ_SAY:
                            processSay(buf, s, si_other, &channelTable, &userTable, serverIp, sport, &subServerTable, &receivedIds);
                            break;
                        case REQ_LIST:
                        	cout<<"\n"<<serverIp<<":"<<sport;
                            processList(s, si_other, &channelTable, &userTable);
                            break;
                        case REQ_WHO:
                        	cout<<"\n"<<serverIp<<":"<<sport;
                            processWho(buf, s, si_other, &channelTable, &userTable);
                            break;
                        case REQ_KEEP_ALIVE:
                            processAlive(si_other, &userTable);  //log suppressed to avoid clutter
                            break;
                        case SREQ_JOIN:
                            cout<<"\n"<<serverIp<<":"<<sport<<" "<<inet_ntoa(si_other.sin_addr)<<":"<<ntohs(si_other.sin_port);
                            cout<<" recv S2S Join ";
                            process_SJoin(buf, s, si_other, serverTable, serverTableSize, &subServerTable, cServer);
                            break;
                        case SREQ_LEAVE:
                            cout<<"\n"<<serverIp<<":"<<sport<<" "<<inet_ntoa(si_other.sin_addr)<<":"<<ntohs(si_other.sin_port);
                            cout<<" recv S2S Leave ";
                            process_SLeave(buf, si_other, &subServerTable);
                            break;
                        case S_SAY:
                            cout<<"\n"<<serverIp<<":"<<sport<<" "<<inet_ntoa(si_other.sin_addr)<<":"<<ntohs(si_other.sin_port);
                            cout<<" recv S2S Say ";
                            process_SSay(buf, s, si_other, &channelTable, serverIp, sport, &subServerTable, &receivedIds);
                            break;
                        default:
                            cout<<"invalid message\n";
                    }
                }
                fflush( stdout );
            }
            else if(FD_ISSET(timer_cleanup, &readfds)){
                it_c.it_value.tv_sec=0;  //disarm timer
                it_c.it_value.tv_nsec=0; //disarm timer
                if (timerfd_settime(timer_cleanup, TFD_TIMER_ABSTIME, &it_c, NULL) == -1)
                    cout<<"timerfd_resettime";
                cleanup(&channelTable, &userTable);
                server_cleanup(&subServerTable);
                //reset timer
                setTimer(timer_cleanup, it_c, CLEANUP);
                /*if (clock_gettime(CLOCK_REALTIME, &now) == -1)
                    cout<<"clock_gettime";
                
                it_c.it_value.tv_sec=now.tv_sec + CLEANUP;
                it_c.it_value.tv_nsec=now.tv_nsec;
                it_c.it_interval.tv_sec=0;
                it_c.it_interval.tv_nsec=0;
                
                if (timerfd_settime(timer_cleanup, TFD_TIMER_ABSTIME, &it_c, NULL) == -1)
                    cout<<"timerfd_settime"; */
            }
            else if(FD_ISSET(timer_join, &readfds)){
                it_j.it_value.tv_sec=0;  //disarm timer
                it_j.it_value.tv_nsec=0; //disarm timer
                if (timerfd_settime(timer_join, TFD_TIMER_ABSTIME, &it_j, NULL) == -1)
                    cout<<"timerfd_resettime";
                
                send_SJoin(s, &subServerTable);   //send soft state join 
                setTimer(timer_join, it_j, SJOIN);
            }
        }
    }
    
    close(s);
    return 0;
}