//
//  HWServer.c
//  HWConsoleServer
//
//  Created by James Lennon on 10/9/14.
//  Copyright (c) 2014 James Lennon. All rights reserved.
//

#include "Server.h"
#include <thread>
#include <sstream>
#include <stdio.h>
#include <cstdio>

using namespace playphone;

void playphone::sendMsg(TCPSocket* sock, Serializable& r){
    const char* msg = r.getJSONString();
    int len = (int)strlen(msg);
    sock->send(msg, len+1);
}

Server::Server(ServerHandler& h): handler(h){
    this->handler = h;
    currentClientID = 0;
    this->shouldRun = true;
    handler.serv = this;
}

void Server::start(){
    TCPServerSocket* serverSock = nullptr;
    unsigned short currentPort = START_PORT;
    while(serverSock==nullptr){
        try {
            //See if currentPort is in use
            serverSock = new TCPServerSocket("0.0.0.0", currentPort);
            break;
        } catch (SocketException ex) {
            if(PP_DEBUG)printf("Port %d in use\n", currentPort);
            currentPort++;
        }
    }
    if(PP_DEBUG)printf("Successfully connected to port %d\n", currentPort);
    handler.onStart();
    listenForSockets(serverSock);
    
    //    thread listenThread(&Server::listenForSockets, this, serverSock);
    //    listenThread.detach();
}

void Server::listenForSockets(TCPServerSocket *serverSock){
    while (shouldRun) {
        //Accept new client, spawn thread to handle it
        TCPSocket *sock = serverSock->accept();
        thread t(&Server::handleClient, this, sock);
        t.detach();
    }
    delete serverSock;
    serverSock = NULL;
}

int Server::getClientID(){
    mut.lock();
    int val = currentClientID++;
    mut.unlock();
    return val;
}

void Server::handleClient(TCPSocket* sock){
    Client cli(sock, getClientID(), this);
    clients.insert(std::pair<int,Client&>(cli.socketID, cli));
    cli.run();
}

bool Server::send(Serializable &s, int clientID){
    map<int, Client&>::iterator it = clients.find(clientID);
    if(it!=clients.end()){
        it->second.send(s);
        return true;
    }
    return false;
}

void Server::broadcast(Serializable &s, int except){
    for(map<int, Client&>::iterator it = clients.begin(); it != clients.end(); ++it){
        if(it->first != except){
            it->second.send(s);
        }
    }
}

void Server::refreshClients(){
    Request req(1);
    Document d;
    Value& obj = req.serializeJSON(d.GetAllocator());
    GameObject g = getGameObject();
    obj.AddMember("game", g.serializeJSON(d.GetAllocator()), d.GetAllocator());
    for(map<int, Client&>::iterator it = clients.begin(); it != clients.end(); ++it){
        sendMsg(it->second.sock, req);
    }
}

Response error(const char* text){return Response(404, text);}

GameObject Server::getGameObject(){
    GameObject gobj;
    gobj.name = handler.getName();
    gobj.desc = handler.getDesc();
    gobj.filledslots = handler.getFilledSlots();
    gobj.openslots = handler.getOpenSlots();
    
    return gobj;
}

Response Server::handleRequest(playphone::Request &r, Client* cli){
    //handle requests here
    try {
        if(r.operation==0){
            //Discovery Request
            Value& cid = (*r.root)["id"];
            cli->clientID = shared_ptr<IDObject>(new IDObject);
            if(!(*cli->clientID).parseJSON(cid))return error("bad id json");
            
            Response resp(200, "OK");
            GameObject gobj;
            gobj.name = handler.getName();
            gobj.desc = handler.getDesc();
            gobj.filledslots = handler.getFilledSlots();
            gobj.openslots = handler.getOpenSlots();
            
            Document d;
            Value& obj = resp.serializeJSON(d.GetAllocator());
            Value& game = gobj.serializeJSON(d.GetAllocator());
            obj.AddMember("game", game, d.GetAllocator());
            
            Value banned;
            banned.SetObject();
            string why;
            bool is = !handler.canJoin(cli, why);
            banned.AddMember("is", is, d.GetAllocator());
            banned.AddMember("why", why, d.GetAllocator());
            obj.AddMember("banned", banned, d.GetAllocator());
            
            return resp;
        }else if(r.operation==2){
            //Join Request
            string tmp;
            bool canJoin = handler.canJoin(cli, tmp) && !cli->hasJoined && handler.getOpenSlots()>0;
            
            Response resp(200,"OK");
            Document d;
            Value& obj = resp.serializeJSON(d.GetAllocator());
            obj.AddMember("accepted", canJoin, d.GetAllocator());
            if(canJoin){
                handler.onJoin(cli);
                cli->hasJoined = true;
                obj.AddMember("padconfig", handler.getDefaultControls().serializeJSON(d.GetAllocator()), d.GetAllocator());
            }
            
            return resp;
        }else if(r.operation == 3){
            //Disconnect Request
            Response resp(200,"OK");
            if(cli->hasJoined){
                handler.onDisconnect(cli);
                cli->hasJoined = false;
            }else{
                Document d;
                Value& obj = resp.serializeJSON(d.GetAllocator());
                obj.AddMember("msg", string("not in game"), d.GetAllocator());
            }
            
            return resp;
        }else if(r.operation == 5){
            //Pad State Change Request
            Response resp(200,"OK");
            if(cli->hasJoined){
                PadUpdateObject update;
                update.parseJSON(*r.root);
                
            }else{
                Document d;
                Value& obj = resp.serializeJSON(d.GetAllocator());
                obj.AddMember("msg", string("not in game"), d.GetAllocator());
            }
            return resp;
        }
        
        return error("unimplemented op");
    }catch(exception ex){
        return error("json missing required field");
    }
}

void Server::handleResponse(playphone::Response &r, Client* cli){
    //handle responses here
    
}

void Server::setControls(playphone::ControlObject &ctrls){
    for(map<int, Client&>::iterator it = clients.begin(); it != clients.end(); ++it){
        it->second.setControls(ctrls);
    }
}

Client::Client(TCPSocket* sock, int id, Server* serv){
    this->socketID = id;
    this->sock = sock;
    this->serv = serv;
    shouldRun = true;
    hasJoined = false;
}

void Client::send(playphone::Serializable &s){
    sendMsg(sock, s);
}

void Client::handleMsg(string in){
    if(in==""){
        //client disconnected
        return;
    }
    
    Request req;
    Response resp;
    if(req.parseJSON(in.c_str())){
        Response resp = serv->handleRequest(req, this);
        sendMsg(sock, resp);
    }else if(resp.parseJSON(in.c_str())){
        serv->handleResponse(resp, this);
    }else{
        Response resp(404,"bad json");
        sendMsg(sock, resp);
    }
}

void Client::run(){
    //TODO: confirm buffer length
    char buf[BUFFER_LENGTH];
    string msg;
    
    while (shouldRun) {
        int amt = sock->recv(buf, BUFFER_LENGTH-1);
        buf[amt] = 0;
        
        int bytesProcessed = 0;
        do {
            int len = (int)strlen(buf+bytesProcessed);
            msg.append(buf+bytesProcessed, len);
            if(bytesProcessed+len<amt){
                handleMsg(msg);
                msg = "";
            }
            bytesProcessed+=len+1;
        } while (bytesProcessed<amt);
    }
    serv->clients.erase(serv->clients.find(socketID));
    delete sock;
    sock=NULL;
}

void Client::setControls(playphone::ControlObject& ctrls){
    Request r(4);
    Document d;
    Value& obj = r.serializeJSON(d.GetAllocator());
    obj.AddMember("padconfig", ctrls.serializeJSON(d.GetAllocator()), d.GetAllocator());
    send(r);
}

void Client::disconnect(string msg){
    Request r(3);
    Document d;
    Value& obj = r.serializeJSON(d.GetAllocator());
    obj.AddMember("msg", msg, d.GetAllocator());
    send(r);
    hasJoined = false;
    shouldRun = false;
}
