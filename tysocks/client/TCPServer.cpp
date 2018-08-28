//
//  TCPServer.cpp
//  tysocks
//
//  Created by TYPCN on 2016/6/13.
//
//

#include <stdio.h>
#include <ev++.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <fcntl.h>
#endif
#include <string>
#include <errno.h>
#include "TCPServer.hpp"
#include <logging.hpp>
#include <TCPHandler.hpp>
#include <utils.hpp>

void TCPServer::io_accept(ev::io &watcher, int revents) {
    if (EV_ERROR & revents) {
        LOG(WARNING) << "Invalid event " << revents;
        return;
    }

    sockaddr *incoming_addr;
    socklen_t incoming_len;

    if(af == AF_INET){
        incoming_len = sizeof(sockaddr_in);
        incoming_addr = (sockaddr *)malloc(incoming_len);
    }else{
        incoming_len = sizeof(sockaddr_in6);
        incoming_addr = (sockaddr *)malloc(incoming_len);
    }

    int client_sd = accept(watcher.fd, incoming_addr, &incoming_len);

    string ip = ip2str(incoming_addr);

    if (client_sd < 0) {
        LOG(WARNING) << "Failed to accept connection from " << ip << ", errno " << errno;
        return;
    }else{
        LOG(INFO) << "New connection " << client_sd << " from " << ip;
    }

    if(conn_list[client_sd]){
        conn_list[client_sd]->shutdown();
    }

    TCPHandler *hdl = new TCPHandler(conn_list, tp_client, client_sd, packet_size, client_sd, true);
    while (1) {
        if(__sync_bool_compare_and_swap(&conn_list[client_sd], NULL, hdl)){
            break;
        }
    }

    free(incoming_addr);
}

void TCPServer::on_data(int fd, const uint8_t *buf, uint32_t len, const struct sockaddr* addr, socklen_t addrlen){
    if(!this){
        return;
    }
    int conn_id = *(int *)buf;
    if(conn_id < 0){
        return;
    }
    TCPHandler *hdl = conn_list[conn_id];
    if(len == sp && hdl){
        LOG(INFO) << "Connection " << conn_id << " closed by remote host";
        hdl->shutdown();
    }else if(hdl){
        hdl->send(buf + sp, len - sp);
    }else{
        //string ip = ip2str(addr);
        LOG(WARNING) << "Received packet with invalid id " << conn_id;
    }
}

void TCPServer::on_event(int fd, int code, const char* msg, const struct sockaddr* addr, socklen_t addrlen){
    if(code == -2002){
        LOG(FATAL) << "Remote not responding , stopping.";
    }else{
        string ip = ip2str(addr);
        LOG(WARNING) << "Event: " << msg << " Code:" << code << " IP:" << ip;
    }
}

TCPServer::TCPServer(TYP *client, string ip, int port, int max_queue, int psize, int max_conn) : packet_size(psize), tp_client(client) {
    conn_list = (TCPHandler **)malloc(sizeof(TCPHandler *) * max_conn);

    client->setRecvCallback<TCPServer, &TCPServer::on_data>(this);
    client->setEventCallback<TCPServer, &TCPServer::on_event>(this);

    sockaddr *addr = NULL;
    int af = str2ip(ip, port, &addr);
    if(af == AF_INET){
        addrlen = sizeof(sockaddr_in);
    }else{
        addrlen = sizeof(sockaddr_in6);
    }

    fd = socket(af, SOCK_STREAM, 0);

    int enable = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *) (void *) &enable, sizeof(int)) < 0){
        LOG(ERROR) << "Failed to set SO_REUSEADDR, errno " << errno;
    }

    if (::bind(fd, addr, addrlen) != 0) {
        LOG(FATAL) << "Failed to bind, errno " << errno;
    }

    set_non_blocking(fd);

    listen(fd, max_queue);

    io.set<TCPServer, &TCPServer::io_accept>(this);
    io.start(fd, ev::READ);

    sio.set<&TCPServer::signal_cb>();
    sio.start(SIGINT);

    free(addr);
}
