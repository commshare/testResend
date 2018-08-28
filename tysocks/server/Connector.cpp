//
//  Connector.cpp
//  tysocks
//
//  Created by TYPCN on 2016/6/16.
//
//

#include "Connector.hpp"
#include <logging.hpp>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>

void Connector::connectDomain(const uint8_t *domain, uint8_t len, uint16_t port){
    // TODO: IPv6 Domain Support
    char name[len + 1];
    memcpy(name, domain, len);
    name[len] = '\0';
    s_port = port;
    
    hasCallback = false;

    // Some browser may send ip address request as domain
    sockaddr *addr = NULL;
    int af = str2ip(name, ntohs(port), &addr);
    if(af == AF_INET || af == AF_INET6){
        fd = this->setupFd(af);
        if(fd == -1) return;
        return this->connectSockaddr(addr, sizeof(*addr));
    }
    LOG(INFO) << "Resolving domain " << &name[0];
    DNS::get()->getAddrInfoAsync(AF_INET, name, std::bind(&Connector::dnsresp, this, std::placeholders::_1));
}

void Connector::dnsresp(std::vector<dns_result *> results){
    if(!__sync_bool_compare_and_swap(&hasCallback, false, true)){
        return;
    }
    if(results.size() == 0){
        LOG(WARNING) << "DNS query got empty results";
        this->replyStatus(SREP_UNSUPPORTED_ADDRTYPE);
        serv->close_connection(connid);
        delete this;
        return;
    }
    dns_result *addr = results.front();
    if(addr->af == AF_INET){
        this->connectV4(addr->data, s_port);
    }else if(addr->af == AF_INET6){
        this->connectV6(addr->data, s_port);
    }else{
        LOG(WARNING) << "DNS query got invalid af";
        this->replyStatus(SREP_UNSUPPORTED_ADDRTYPE);
        serv->close_connection(connid);
        delete this;
    }
}

int Connector::setupFd(int af){
    fd = socket(af, SOCK_STREAM, 0);
    if(fd < 0){
        LOG(ERROR) << "Unable to create fd, errno " << errno;
        serv->close_connection(connid);
        delete this;
    }else{
        set_non_blocking(fd);
    }
    return fd;
}

void Connector::connectV4(const uint8_t *ip, uint16_t port){
    fd = this->setupFd(AF_INET);
    if(fd == -1) return;
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    memcpy(&addr.sin_addr.s_addr, ip, 4);
    addr.sin_port = port;
    addr.sin_family = AF_INET;

    this->connectSockaddr((struct sockaddr *)&addr, sizeof(addr));
}

void Connector::connectV6(const uint8_t *ip, uint16_t port){
    fd = this->setupFd(AF_INET6);
    if(fd == -1) return;
    sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    memcpy(&addr.sin6_addr.s6_addr, ip, 16);
    addr.sin6_port = port;
    addr.sin6_family = AF_INET6;

    this->connectSockaddr((struct sockaddr *)&addr, sizeof(addr));
}

void Connector::connectSockaddr(struct sockaddr *addr, socklen_t addrlen){
    string ipstr = ip2str(addr);
    LOG(INFO) << "Fd " << fd << " connecting to " << ipstr;
    
    int rv = connect(fd, addr, addrlen);
    if(rv < 0 && errno != EINPROGRESS){
        LOG(ERROR) << "Unable to connect, errno " << errno;
        this->replyStatus(SREP_GENEIC_FAILURE);
        serv->close_connection(connid);
        delete this;
        return;
    }else if(rv == 0){
        this->createTCPHandler();
    }else{
        io.set<Connector, &Connector::callback>(this);
        io.start(fd, ev::WRITE);
        timer.set<Connector, &Connector::timeout>(this);
        timer.start(timeout_s);
    }
}

void Connector::timeout(ev::timer &timer, int revents){
    io.stop();
    timer.stop();
    close(fd);
    this->replyStatus(SREP_TIMED_OUT);
    serv->close_connection(connid);
    LOG(INFO) << "Fd " << fd << " connect timed out";
    delete this;
}

void Connector::callback(ev::io &watcher, int revents){
    io.stop();
    timer.stop();
    int err = 0;
    socklen_t len = sizeof(err);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
    if(err){
        LOG(INFO) << "Fd " << fd << " connect failed with error " << err;
        close(fd);
        this->replyStatus(SREP_CONNECTION_REFUSED);
        serv->close_connection(connid);
        delete this;
        return;
    }
    this->createTCPHandler();
}

void Connector::createTCPHandler(){
    if(serv->conn_list[connid]){
        serv->conn_list[connid]->shutdown();
    }
    this->replyStatus(SREP_SUCCESS);
    TCPHandler *hdl = new TCPHandler(serv->conn_list, serv->tp_server, fd, serv->pktSize, connid, false);
    while (1) {
        if(__sync_bool_compare_and_swap(&serv->conn_list[connid], NULL, hdl)){
            break;
        }
    }
}

void Connector::replyStatus(uint8_t status){
    uint8_t *reply = (uint8_t *)malloc(14);
    memset(reply, 0, 14);
    memcpy(reply, &connid, 4);
    reply[4] = 0x05;
    reply[5] = status;
    reply[7] = 0x01;
    serv->tp_server->send(reply, 14);
}
