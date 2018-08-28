//
//  Connector.hpp
//  tysocks
//
//  Created by TYPCN on 2016/6/16.
//
//

#ifndef Connector_hpp
#define Connector_hpp

#include <stdio.h>
#include <stdint.h>
#include <ev++.h>
#include "DNS.hpp"
#include "S5Server.hpp"

enum Socks5REP{
    SREP_SUCCESS = 0x00,
    SREP_GENEIC_FAILURE = 0x01,
    SREP_NOT_ALLOWED = 0x02,
    SREP_NETWORK_UNREACHABLE = 0x03,
    SREP_HOST_UNREACHABLE = 0x04,
    SREP_CONNECTION_REFUSED = 0x05,
    SREP_TIMED_OUT = 0x06,
    SREP_UNSUPPORTED_COMMAND = 0x07,
    SREP_UNSUPPORTED_ADDRTYPE = 0x08
};

class Connector{
private:
    int fd;
    int connid;
    bool hasCallback;
    uint16_t s_port;
    double timeout_s;
    
    ev::io io;
    ev::timer timer;
    
    S5Server *serv;

    void createTCPHandler();
    void timeout(ev::timer &timer, int revents);
    void callback(ev::io &watcher, int revents);
    void dnsresp(std::vector<dns_result *> results);
    int setupFd(int af);
    
public:
    Connector(S5Server *parent, int connid) : connid(connid) , serv(parent) {
        timeout_s = (double)parent->timeout_ms / 1000.0;
    };
    void connectDomain(const uint8_t *domain, uint8_t len, uint16_t port);
    void connectV4(const uint8_t *ip, uint16_t port);
    void connectV6(const uint8_t *ip, uint16_t port);
    void connectSockaddr(struct sockaddr *addr, socklen_t addrlen);

    void replyStatus(uint8_t status);
};

#endif /* Connector_hpp */
