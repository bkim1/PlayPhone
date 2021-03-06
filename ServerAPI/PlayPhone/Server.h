//
//  HWPPServer.h
//  HWConsoleServer
//
//  Created by James Lennon on 10/9/14.
//  Copyright (c) 2014 James Lennon. All rights reserved.
//

#ifndef Server_H
#define Server_H

#include "PracticalSocket.h"
#include "PlayPhone.h"
#include <mutex>
#include <map>


namespace playphone {
    
    class Client;
    class Server;
    class Serializable;
    class Request;
    class Response;
    class ServerHandler;
    class IDObject;
    class GameObject;
    class ControlObject;
    
    void sendMsg(TCPSocket* sock, Serializable& r);
    
    class Client {
        
    public:
        int socketID;
        TCPSocket* sock;
        Server* serv;
        bool shouldRun, hasJoined;
        shared_ptr<IDObject> clientID;
        
        Client(TCPSocket* sock, int sockID, Server* serv);
        void send(Serializable &s);
        void run();
        void setControls(ControlObject& ctrls);
        void disconnect(string msg);
    private:
        void handleMsg(string msg);
    };
    
    class Server {
        
    public:
        int currentClientID;
        map<int, Client&> clients;
        bool shouldRun;
        
        Server(ServerHandler& handler);
        void start();
        void refreshClients();
        void setControls(ControlObject& ctrls);
        
        Response handleRequest(Request& r, Client* cli);
        void handleResponse(Response& r, Client* cli);
    private:
        mutex mut;
        ServerHandler& handler;
        
        bool send(Serializable& s, int clientID);
        void broadcast(Serializable &s, int except=-1);
        int getClientID();
        void handleClient(TCPSocket* sock);
        void listenForSockets(TCPServerSocket *serverSock);
        GameObject getGameObject();
    };
    
}
#endif /* defined(__HWConsoleServer__HWPPServer__) */
