//
//  TCPServer.hpp
//  tysocks
//
//  Created by TYPCN on 2016/6/12.
//
//

#ifndef tcp_hpp
#define tcp_hpp

#include <stdio.h>
#include <unistd.h>
#include <vector>
#include <TYP.hpp>

#ifdef _WIN32
#define SHUT_RDWR SD_BOTH
#endif

class TCPHandler;

using namespace std;

class TCPServer {
private:
    ev::io io;
    ev::sig sio;
    int fd;
    int af;
    int packet_size;
    socklen_t addrlen;
    TYP *tp_client;

public:

    TCPHandler **conn_list;

    TCPServer(TYP* client, string ip, int port, int max_queue, int psize, int max_conn);
    void io_accept(ev::io &watcher, int revents);
    void on_data(int fd, const uint8_t *buf, uint32_t len, const struct sockaddr* addr, socklen_t addrlen);
    void on_event(int fd, int code, const char* msg, const struct sockaddr* addr, socklen_t addrlen);

    static void signal_cb(ev::sig &signal, int revents) {
        signal.loop.break_loop();
    }

    virtual ~TCPServer() {
        free(conn_list);
        io.stop();
        sio.stop();
        ::shutdown(fd, SHUT_RDWR);
        ::close(fd);
    }
};

#endif /* tcp_hpp */
