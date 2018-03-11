#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>       //required for perror
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

#include <iostream>
#include "duckchat.h"

using namespace std;


#define NO_CMD 6
#define MAX_CMD_SZ 6

/*------------------------COMMAND TYPES-------------------------------------*/
#define LIST 0
#define EXIT 1
#define JOIN 2
#define LEAVE 3
#define WHO 4
#define SWITCH 5

/*------------------------Prototypes-----------------------------------------*/
void displaySay(char* buf);
void displayList(char* buf);
void displayWho(char* buf);
void displayError(char* buf);



/*-----------------------------------------------------------------------------
 * Method: usage(..)
 * Scope: local
 *---------------------------------------------------------------------------*/
void usage(char* argv0)
{
    //cout<<"DuckChat Client\n";
    cout<<"Usage: "<< argv0 << " server_socket server_port username\n";
    //cout<<"Example: "<<argv0<<" ix 8500 ss \n";
} /* -- usage -- */

/*-----------------------------------------------------------------------------
 * Method: hostname to IP address
 * Scope: local TODO: remove it from here and serverutils.h and place in a separate file
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
 * Method: Tokenize Command
 * Scope: local  TODO: add error checking for wrong command entered. see how solution binaries are working
 * Parameters: Input String, Pointer to output array
 * Ruturns: Number of tokens
 *---------------------------------------------------------------------------*/
int tokCommand(char* command, char Tokens[][CHANNEL_MAX])
{
    char s[CHANNEL_MAX];
    char* check;
    memset(s, '\0', sizeof(s));
    strcpy(s, command);
    strtok(s, "\n");
    int count = 0;
    check = strtok(s, " ");
    if(check){
        if(strlen(check) > CHANNEL_MAX-1){  //strlen does not count null char so leave space for that
            return 0;
        }
        
        stpcpy(Tokens[count], check);
    }
    while (check) {
        count++;
        //cout<<"token"<<count<<": "<< Tokens[count-1]<<"\n";   //count-1 is just to give correct index
        check = strtok(NULL, " ");
        
        if(check && (count < 2)){
            
            if(strlen(check) > CHANNEL_MAX-1){
                return 0;
            }
            stpcpy(Tokens[count], check);
        }
    }
    return count;
}	/* -- tokCommand -- */

/*-----------------------------------------------------------------------------
 * Method: checkCommandType
 * Scope: local
 *---------------------------------------------------------------------------*/
int checkCommandType(char* command)
{
    //cout<<"within checkCommandType()\n";
    int type = 99;
    int found = 0;
    char Commands[NO_CMD][MAX_CMD_SZ];
    strcpy(Commands[0],"list");
    strcpy(Commands[1], "exit");
    strcpy(Commands[2], "join");
    strcpy(Commands[3], "leave");
    strcpy(Commands[4], "who");
    strcpy(Commands[5], "switch");
    int i;
    for(i = 0; (i < NO_CMD) && !found; i++) {
        if(strcmp(command, Commands[i]) == 0)
        {
            found = 1;
            type = i;
        }
    }
    return type;
}	/* -- checkCommandType -- */

/*-----------------------------------------------------------------------------
 * Method: processSwitch
 * Scope: local
 *---------------------------------------------------------------------------*/
void processSwitch(char* channel, std::set<string> *chSet, char* curChannel)
{
    //cout<<"within processSwitch()\n";
    std::set<string>::iterator it;
    it = (*chSet).find(channel);
    if(it == (*chSet).end())
        cout<<"\nYou were not subscribed to channel "<<channel;
    else
        strcpy(curChannel, channel);
}	/* -- processSwitch -- */

/*-----------------------------------------------------------------------------
 * Method: keepAlive
 * Scope: local
 *---------------------------------------------------------------------------*/
void keepAlive(int s, struct sockaddr_in cServer)
{
    //cout<<"within keepAlive()\n";
    request_keep_alive m7;
    memset(&m7, 0, sizeof(m7)) ;
    m7.req_type = REQ_KEEP_ALIVE;
    if (sendto(s, &m7, sizeof(m7), 0, (struct sockaddr *)&cServer, sizeof(cServer))==-1)
        diep("sendto()");
}	/* -- keepAlive -- */

///////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

void displayReceived(char* buf)
{
    text* r = (struct text *)buf;
    switch ((*r).txt_type)
    {
        case TXT_SAY:
            displaySay(buf);
            break;
        case TXT_LIST:
            displayList(buf);
            break;
        case TXT_WHO:
            displayWho(buf);
            break;
        case TXT_ERROR:
            displayError(buf);
            break;
        default:
            cout<<"*Unknown text from server\n";
    }
    
}	/* -- displayReceived -- */


/*-----------------------------------------------------------------------------
 * Method: displaySay
 * Scope: local
 *---------------------------------------------------------------------------*/
void displaySay(char* buf){
    text_say* r0 = (struct text_say *)buf;
    cout<<"["<<(*r0).txt_channel<<"]";
    cout<<"["<<(*r0).txt_username<<"]";
    cout<<(*r0).txt_text;
}   /* -- displaySay -- */

/*-----------------------------------------------------------------------------
 * Method: displayList
 * Scope: local
 *---------------------------------------------------------------------------*/
void displayList(char* buf){
    text_list* r1 = (struct text_list *)buf;
    cout<<"Existing channels:";
    for(int i = 0; i < (*r1).txt_nchannels ; i++){
        cout<<"\n"<<(*r1).txt_channels[i].ch_channel;
    }
}   /* -- displayList -- */
/*-----------------------------------------------------------------------------
 * Method: displayWho
 * Scope: local
 *---------------------------------------------------------------------------*/
void displayWho(char* buf){
    text_who* r2 = (struct text_who *)buf;
    cout<<"Users on channel "<<(*r2).txt_channel<<":";
    for(int i = 0; i < (*r2).txt_nusernames ; i++){
        cout<<"\n"<<(*r2).txt_users[i].us_username;
    }
    
}   /* -- displayWho -- */
/*-----------------------------------------------------------------------------
 * Method: displayError
 * Scope: local
 *---------------------------------------------------------------------------*/
void displayError(char* buf){
    text_error* r3 = (struct text_error *)buf;
    cout<<"Error: "<<(*r3).txt_error;
}   /* -- displayError -- */