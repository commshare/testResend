//
//  S5Server.hpp
//  tysocks
//
//  Created by TYPCN on 2016/6/15.
//
//

#ifndef S5Server_hpp
#define S5Server_hpp

#include <stdio.h>
#include <unistd.h>
#include <ev++.h>
#include <mutex>
#include <condition_variable>
#include <TCPHandler.hpp>


class S5Server {
private:
    std::mutex running_mtx;
    std::condition_variable running_cv;
    ev::loop_ref loop_ref;
    ev::timer hb_timer;
    int max_conn;
    bool stop_loop;
    void heartbeat(ev::timer &timer, int revents);
    void process_control_packet(const uint8_t *buf, uint32_t len);
    
public:
    int pktSize;
    int timeout_ms;
    TYP *tp_server;
    TCPHandler **conn_list;
    
    S5Server(TYP* server, ev::loop_ref loop, int psize, int max_conn, int conn_timeout);

    void on_data(int fd, const uint8_t *buf, uint32_t len, const struct sockaddr* addr, socklen_t addrlen);
    void on_event(int fd, int code, const char* msg, const struct sockaddr* addr, socklen_t addrlen);
    void close_connection(int id);
    void run();
};


#endif /* S5Server_hpp */
