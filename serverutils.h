#include "duckchat.h"
#include "serverhelpers.h"

/*-----------------------------------------------------------------------------
 * Method: sendError
 *---------------------------------------------------------------------------*/
void sendError(char* e, int s, sockaddr_in client){
    text_error t3;
    t3.txt_type = TXT_ERROR;
    strcpy(t3.txt_error, e);
    if (sendto(s, &t3, sizeof(t3), 0, (struct sockaddr *)&client, sizeof(client))==-1)
        diep("sendError-sendto()");
}   /* -- sendError -- */
/*-----------------------------------------------------------------------------
 * Method: sendBroadcast
 * sender is used to check if sent from a server and  then not broadcast to that server
 *---------------------------------------------------------------------------*/
void sendBroadcastJoin(int s, char* channel, sockaddr_in serverTable[], int serverTableSize, sockaddr_in sender, subscribedTable *subTable, sockaddr_in server) {
    s_join s2s;
    s2s.req_type = SREQ_JOIN;
    strcpy(s2s.req_channel, channel);
    for (int i = 0; i < serverTableSize; i++) {
        subServer srv;
        srv.ipInfo = serverTable[i];
        srv.lastSent = time(0);
        (*subTable)[channel].insert(srv);  //insert everyone in server table
        if(!cmp_sockaddr(sender, serverTable[i])) {   //do not send join to sender
            cout<<"\n"<<inet_ntoa(server.sin_addr)<<":"<<ntohs(server.sin_port)<<" "<<inet_ntoa(serverTable[i].sin_addr)<<":"<<ntohs(serverTable[i].sin_port);
            cout<<" send S2S Join "<<channel;
            if (sendto(s, &s2s, sizeof(s2s), 0, (struct sockaddr *)&(serverTable[i]), sizeof(serverTable[i]))==-1)
                diep("sendBroadcastJoin-sendto()");
        }
    }
}   /* -- sendBroadcast -- */

/*-----------------------------------------------------------------------------
 * Method: processAlive
 *---------------------------------------------------------------------------*/
void processAlive(sockaddr_in client, users *userTable){
    //if(userTable->find(client) != userTable->end())    //doing this in server.cpp
    (*userTable)[client].lastSent = time(0);
    //cout<<"\nserver: "<<(*userTable)[client].userName<<" keeps alive";  
    
}/* -- processAlive -- */
/*-----------------------------------------------------------------------------
 * Method: processSay
 *---------------------------------------------------------------------------*/
void processSay(char* buf, int s, sockaddr_in client, channelMap *chTable, users *userTable, char serverIp[], int& sport, subscribedTable* subTable, idSet* rids ){
    request_say* r4 = (struct request_say*)buf;
    if(strlen(r4->req_text) > SAY_MAX-1){
        cout<<"\nserver: "<<(*userTable)[client].userName<<" sends **bad message in "<<r4->req_channel;
        char e[SAY_MAX]  = "**Bad message-Discarded";
        sendError(e,s,client);
        return;
    }
    cout<<"\n"<<serverIp<<":"<<sport<<" "<<inet_ntoa(client.sin_addr)<<":"<<ntohs(client.sin_port);
    cout<<" recv Request Say "<<r4->req_channel<<" \""<<r4->req_text<<"\"";
    (*userTable)[client].lastSent = time(0);
    char chName[CHANNEL_MAX];
    strcpy(chName, r4->req_channel);
    if(strcmp(chName, " ") != 0)  {
        //user has some active channel
        text_say t0;
        t0.txt_type = TXT_SAY;
        strcpy(t0.txt_channel, chName);
        strcpy(t0.txt_username, (*userTable)[client]. userName);
        strcpy(t0.txt_text, r4->req_text);
        for (sockSet::iterator itU=((*chTable)[chName]).begin(); itU != ((*chTable)[chName]).end(); ++itU) {
            cout<<"\n"<<serverIp<<":"<<sport<<" "<<inet_ntoa(itU->sin_addr)<<":"<<ntohs(itU->sin_port);
            cout<<" send Say "<<chName<<"\""<<r4->req_text<<"\"";
            if (sendto(s, &t0, sizeof(t0), 0, (struct sockaddr *)&(*itU), sizeof(*itU))==-1)
                diep("processSay-sendto()");
        }
        
        //now send to other servers
        s_say s10;
        s10.req_type = S_SAY;
        s10.say_id = genRandom();
        rids->insert(s10.say_id);
        strcpy(s10.txt_username, (*userTable)[client].userName);
        strcpy(s10.req_channel, r4->req_channel);
        strcpy(s10.req_text, r4->req_text);
        for (serverSet::iterator itU=((*subTable)[chName]).begin(); itU != ((*subTable)[chName]).end(); ++itU)  {
            cout<<"\n"<<serverIp<<":"<<sport<<" "<<inet_ntoa(itU->ipInfo.sin_addr)<<":"<<ntohs(itU->ipInfo.sin_port);
            cout<<" send S2S Say "<<r4->req_channel<<"\""<<r4->req_text<<"\"";
            if (sendto(s, &s10, sizeof(s10), 0, (struct sockaddr *)&(itU->ipInfo), sizeof(itU->ipInfo))==-1)
                diep("processSay-sendto()");
        }
    }
    else
        cout<<"\nserver: "<<(*userTable)[client].userName<<" trying to send a message to channel Common where he/she is not a member";
    
}/* -- processSay -- */

/*-----------------------------------------------------------------------------
 * Method: process_SSay
 *---------------------------------------------------------------------------*/
void process_SSay(char* buf, int s, sockaddr_in sender, channelMap *chTable, char serverIp[], int& sport, subscribedTable* subTable, idSet* rids){
    s_say* s10 = (struct s_say*)buf;
    cout<<s10->req_channel<<" \""<<s10->req_text<<"\"";
    if(rids->find(s10->say_id) == rids->end())  {
        //dint find this receive id in database
        rids->insert(s10->say_id);
        char chName[CHANNEL_MAX];
        strcpy(chName, s10->req_channel);
        text_say t0;
        t0.txt_type = TXT_SAY;
        strcpy(t0.txt_channel, chName);
        strcpy(t0.txt_username, s10->txt_username);
        strcpy(t0.txt_text, s10->req_text);
        for (sockSet::iterator itU=((*chTable)[chName]).begin(); itU != ((*chTable)[chName]).end(); ++itU) {
            cout<<"\n"<<serverIp<<":"<<sport<<" "<<inet_ntoa(itU->sin_addr)<<":"<<ntohs(itU->sin_port);
            cout<<" send Say "<<s10->req_channel<<"\""<<s10->req_text<<"\"";
            fflush( stdout );
            if (sendto(s, &t0, sizeof(t0), 0, (struct sockaddr *)&(*itU), sizeof(*itU))==-1)
                diep("process_SSay-sendto()");
        }
        
        //now send to other servers
        for (serverSet::iterator itU=((*subTable)[chName]).begin(); itU != ((*subTable)[chName]).end(); ++itU)
            if(!cmp_sockaddr(itU->ipInfo, sender)) {
                cout<<"\n"<<serverIp<<":"<<sport<<" "<<inet_ntoa(itU->ipInfo.sin_addr)<<":"<<ntohs(itU->ipInfo.sin_port);
                cout<<" send S2S Say "<<s10->req_channel<<"\""<<s10->req_text<<"\"";
                fflush( stdout );
                if (sendto(s, s10, sizeof(*s10), 0, (struct sockaddr *)&(itU->ipInfo), sizeof(itU->ipInfo))==-1)
                    diep("process_SSay-sendto()");
            }
        //Do lazy leave
        //if no user and only one server
        if((chTable->find(s10->req_channel) == chTable->end() || ((*chTable)[chName].size() == 0)) && ((*subTable)[chName].size() <= 1))
        {
            s_leave s9;
            s9.req_type = SREQ_LEAVE;
            strcpy(s9.req_channel, s10->req_channel);
            cout<<"\n"<<serverIp<<":"<<sport<<" "<<inet_ntoa(sender.sin_addr)<<":"<<ntohs(sender.sin_port);
            cout<<" send S2S Leave "<<s10->req_channel;
            fflush( stdout );
            if (sendto(s, &s9, sizeof(s9), 0, (struct sockaddr *)&sender, sizeof(sender))==-1)
                diep("process_SSay-sendto()");
            serverSet::iterator itU = (*subTable)[s9.req_channel].begin();
            if(itU != (*subTable)[s9.req_channel].end() )
                (*subTable)[s9.req_channel].erase(itU);
            subTable->erase(s9.req_channel);
            
        }
        
    }
    else {
        cout<<"\n"<<serverIp<<":"<<sport<<" found loop";
        //Find sayID in database so send Leave to avoid loops
        s_leave s9;
        s9.req_type = SREQ_LEAVE;
        strcpy(s9.req_channel, s10->req_channel);
        cout<<"\n"<<serverIp<<":"<<sport<<" "<<inet_ntoa(sender.sin_addr)<<":"<<ntohs(sender.sin_port);
        cout<<" send S2S Leave "<<s10->req_channel;
        fflush( stdout );
        if (sendto(s, &s9, sizeof(s9), 0, (struct sockaddr *)&sender, sizeof(sender))==-1)
            diep("process_SSay-sendto()");
        subServer temp;
        temp.ipInfo = sender;
        temp.lastSent = time(0);
        serverSet::iterator itU = (*subTable)[s9.req_channel].find(temp);
        if(itU != (*subTable)[s9.req_channel].end() )
            (*subTable)[s9.req_channel].erase(itU);
    }
}/* -- process_SSay -- */

/*-----------------------------------------------------------------------------
 * Method: processLeave
 *---------------------------------------------------------------------------*/
void processLeave(char* buf, int s, sockaddr_in client, channelMap *chTable, users *userTable){
    request_leave* r3 = (request_leave*)buf;
    (*userTable)[client].lastSent = time(0);
    
    text_error t3;
    t3.txt_type = TXT_ERROR;
    
    if(chTable->find(r3->req_channel) == chTable->end()){
        cout<<" "<<(*userTable)[client].userName<<" trying to leave non-existent channel "<<r3->req_channel;
        char e[SAY_MAX] = "No channel by  the name ";
        strcat(e,r3->req_channel);
        strcpy(t3.txt_error, e);
        if (sendto(s, &t3, sizeof(t3), 0, (struct sockaddr *)&client, sizeof(client))==-1)
            diep("processLeave-sendto()");
        return;
    }
    else if((*chTable)[r3->req_channel].find(client) == (*chTable)[r3->req_channel].end()){
        cout<<" "<<(*userTable)[client].userName<<" trying to leave channel "<<r3->req_channel<<" where he/she is not a member";
        char e[SAY_MAX] = "You are not in channel ";
        strcat(e,r3->req_channel);
        strcpy(t3.txt_error, e);
        if (sendto(s, &t3, sizeof(t3), 0, (struct sockaddr *)&client, sizeof(client))==-1)
            diep("processLeave-sendto()");
        return;
    }
    else{
        cout<<" "<<(*userTable)[client].userName<<" leaves channel "<<r3->req_channel;
        (*chTable)[r3->req_channel].erase(client);
        if((*chTable)[r3->req_channel].size() == 0)   //if no users, delete channel
            chTable->erase(r3->req_channel);
        if(strcmp((*userTable)[client].channelName, r3->req_channel) == 0)
            strcpy((*userTable)[client].channelName, " ");
        
    }
}/* -- processLeave -- */
/*-----------------------------------------------------------------------------
 * Method: process_SLeave
 * Process a received Leave message from another server
 *---------------------------------------------------------------------------*/
void process_SLeave(char* buf, sockaddr_in sender, subscribedTable* subTable ){
    s_leave* s9 = (s_leave*)buf;
    subServer temp;
    temp.ipInfo = sender;
    temp.lastSent = time(0);
    serverSet::iterator itU = (*subTable)[s9->req_channel].find(temp);
    if(itU != (*subTable)[s9->req_channel].end() ) {
        (*subTable)[s9->req_channel].erase(itU);
        if((*subTable)[s9->req_channel].size() == 0)
        	subTable->erase(s9->req_channel);
    }
    
}/* -- process_SLeave -- */
/*-----------------------------------------------------------------------------
 * Method: processList
 *---------------------------------------------------------------------------*/
void processList(int s, sockaddr_in client, channelMap *chTable, users *userTable){
    cout<<" "<<(*userTable)[client].userName<<" lists channels";
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
    for (channelMap::iterator it=chTable->begin(); it != chTable->end(); ++it,i++)
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
void processWho(char* buf, int s, sockaddr_in client, channelMap *chTable, users *userTable ){
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
    cout<<" "<<(*userTable)[client].userName<<" lists users in channel "<<t2.txt_channel;
    fflush( stdout );
    user_info* txt_users = new user_info[t2.txt_nusernames];
    
    size_t sendSize = sizeof(t2) + (t2.txt_nusernames * sizeof(user_info));
    char* sendbuf = new char[sendSize];
    memcpy(sendbuf, &t2, sizeof(t2));
    
    int i = 0;
    for (sockSet::iterator itU=((*chTable)[t2.txt_channel]).begin(); itU != ((*chTable)[t2.txt_channel]).end(); ++itU, i++)
        strcpy(txt_users[i].us_username, (*userTable)[(*itU)].userName);
    
    memcpy((sendbuf+sizeof(t2)), txt_users, t2.txt_nusernames * sizeof(user_info));
    if (sendto(s, sendbuf, sendSize, 0, (struct sockaddr *)&client, sizeof(client))==-1)
        diep("processWho()");
    
    delete[] txt_users;
    delete[] sendbuf;
}/* -- procesWho -- */
/*-----------------------------------------------------------------------------
 * Method: processJoin
 *---------------------------------------------------------------------------*/
void processJoin(char* buf, int s, sockaddr_in client, sockaddr_in serverTable[ ], int sTableSz, channelMap *chTable, users *userTable, subscribedTable *subTable, sockaddr_in server){
    
    (*userTable)[client].lastSent = time(0);
    request_join* r1 = (struct request_join *)buf;
    cout<<" "<<(*userTable)[client].userName<<" joins channel "<<r1->req_channel;
    (*chTable)[r1->req_channel].insert(client);
    strcpy((*userTable)[client].channelName, r1->req_channel);
    if((subTable->find(r1->req_channel) == subTable->end()) || ((*subTable)[r1->req_channel].size() == 0))   //do extra
        sendBroadcastJoin(s, r1->req_channel, serverTable, sTableSz, client, subTable, server);
}/* -- procesJoin -- */
/*-----------------------------------------------------------------------------
 * Method: processLogin
 *---------------------------------------------------------------------------*/
void processLogin(char* buf, int s, sockaddr_in client, users *userTable ){
    user u;
    request_login* r0 = (struct request_login *)buf;
    if(strlen(r0->req_username) > USERNAME_MAX-1){
        char e[SAY_MAX]  = "Bad user name";
        sendError(e,s,client);
        return;
    }
    strcpy(u.userName, r0->req_username);
    cout<<" "<<u.userName<<" logs in";
    u.ipInfo = client;
    u.lastSent = time(0);
    (*userTable)[client] = u;
}/* -- procesLogin -- */
/*-----------------------------------------------------------------------------
 * Method: processLogout
 *---------------------------------------------------------------------------*/
void processLogout(sockaddr_in server, sockaddr_in client, channelMap *chTable, users *userTable ){
    if(userTable->find(client) != userTable->end()){
        cout<<"\n"<<inet_ntoa(server.sin_addr)<<":"<<ntohs(server.sin_port)<<" "<<(*userTable)[client].userName<<" logs out";
        (*userTable).erase(client);
        channelMap::iterator itu=chTable->begin();
        while(itu != chTable->end()){
            if((itu->second).find(client) !=  (itu->second).end())
                (itu->second).erase(client);
            if((itu->second).size() == 0) {  //if no users, delete channel
                channelMap::iterator eraseitu = itu;
                ++itu;
                chTable->erase(eraseitu);
            }
            else
                ++itu;
        }
    }
}/* -- procesLogout -- */
/*-----------------------------------------------------------------------------
 * Method: process_SJoin
 *---------------------------------------------------------------------------*/
void process_SJoin(char* buf, int s, sockaddr_in sender, sockaddr_in serverTable[],int sTableSz, subscribedTable *subTable, sockaddr_in server) {
    s_join* s8 = (struct s_join* )buf;
    cout<<s8->req_channel;   //rest of printing in caller
    if(subTable->find(s8->req_channel) == subTable->end() || (*subTable)[s8->req_channel].size() == 0)
        sendBroadcastJoin(s, s8->req_channel, serverTable, sTableSz, sender, subTable, server);
    else {
        subServer temp;
        temp.ipInfo = sender;
        temp.lastSent = time(0);
        serverSet::iterator itU = (*subTable)[s8->req_channel].find(temp);
        if(itU != (*subTable)[s8->req_channel].end() ) 
            (*subTable)[s8->req_channel].erase(itU);    //received soft state join so erase last entry if existed
        (*subTable)[s8->req_channel].insert(temp);
    }
}
/*-----------------------------------------------------------------------------
 * Method: send_SJoin
 * Soft state join messages
 *---------------------------------------------------------------------------*/
//void send_SJoin(int s, char serverIp[], int& sport, subscribedTable *subTable){
void send_SJoin(int s, subscribedTable *subTable){
    s_join s8;
    s8.req_type = SREQ_JOIN;
    //cout<<"\n-----------------SOFT STATE BEGINS------------------------------------";
    for (subscribedTable::iterator it=subTable->begin(); it != subTable->end(); ++it){
        //cout <<"\n broadcasting on Channel: "<<it->first<<"-"<<(it->second).size()<<" Server(s)";
        strcpy(s8.req_channel, (it->first).c_str());
        for (serverSet::iterator itU=(it->second).begin(); itU != (it->second).end(); ++itU){
            //cout<<"\n"<<serverIp<<":"<<sport<<" "<<inet_ntoa(itU->ipInfo.sin_addr)<<":"<<ntohs(itU->ipInfo.sin_port);
            //cout<<" send S2S Join "<<s8.req_channel;
            if (sendto(s, &s8, sizeof(s8), 0, (struct sockaddr *)&(itU->ipInfo), sizeof(itU->ipInfo))==-1)
                diep("process_SJoin-sendto()");
            
        }
    }
    //cout<<"\n-----------------SOFT STATE ENDS--------------------------------------";
}
/*-----------------------------------------------------------------------------
 * Method: cleanup
 * User cleanup for inactive users
 *---------------------------------------------------------------------------*/
void cleanup(channelMap *chTable, users *userTable){
    time_t now = time(0);
    users::iterator it=userTable->begin();
    while(it != userTable->end()){
        if(difftime(now,(it->second).lastSent) >= CLEANUP){
            users::iterator eraseit= it;
            sockaddr_in client = eraseit->first;
            ++it;
            cout<<"\nserver: forcibly removing user "<<(eraseit->second).userName;
            fflush( stdout );
            userTable->erase(eraseit);
            channelMap::iterator itu=chTable->begin();
            while(itu != chTable->end()){
                if((itu->second).find(client) !=  (itu->second).end())
                    (itu->second).erase(client);
                if((itu->second).size() == 0) {  //if no users, delete channel
                    channelMap::iterator eraseitu = itu;
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
/*-----------------------------------------------------------------------------
 * Method: server_cleanup
 *---------------------------------------------------------------------------*/
void server_cleanup(subscribedTable *subTable) {
    time_t now = time(0);
    subscribedTable::iterator it = subTable->begin();
    while(it != subTable->end()) {
        serverSet::iterator itu = (it->second).begin();
        while(itu != (it->second).end()) {
            if(difftime(now,itu->lastSent) >= CLEANUP) {
                serverSet::iterator its = itu;
                ++itu;
                (it->second).erase(its);
            }
            else
                ++itu;
        }
        
        ++it;
    }
}