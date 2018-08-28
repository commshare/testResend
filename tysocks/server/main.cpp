#include <string>
#include <iostream>
#include <ev++.h>
#include <typroto/typroto.h>
#include <logging.hpp>
#include <INIReader.hpp>
#include <TYP.hpp>
#include "S5Server.hpp"
#include "DNS.hpp"

DNS* DNS::dns_ = NULL;

using namespace std;

CREATE_LOGGER;

int main(int argc, char *argv[]){
    INIT_LOGGER;
    string cfgpath = "/etc/tysocks/server";
    if (argc > 1)
        cfgpath = argv[1];

    INIReader reader(cfgpath);
    if (reader.ParseError() < 0) {
        cout << "Usage: ./tysocks_server <config file path>" << endl;
        return 1;
    }

    // Setup typroto
    int rv = tp_init();
    if(rv != 0){
        LOG(FATAL) << "Failed to init typroto";
    }

    // Setup libev
    setevbackend(reader.Get("TCP", "backend", "select"));
    ev::default_loop loop;

    // Setup DNS
    double dns_timeout = (double)reader.GetInteger("DNS", "timeout", 3000) / 1000.0;
    int dns_repeat = reader.GetInteger("DNS", "repeat", 2);
    DNS::get()->init(loop.raw_loop, dns_timeout, dns_repeat);

    // Server config
    string server_ip = reader.Get("Server", "bind", "0.0.0.0");
    int server_port = reader.GetInteger("Server", "port", 443);
    string psk = reader.Get("Server", "key", "");
    int psize = reader.GetInteger("Server", "pktSize", 1400);

    int max_conn = reader.GetInteger("TCP", "maxconn", 8192);
    int conn_timeout = reader.GetInteger("TCP", "conntimeout", 5000);

    // Create typroto server
    TYP *server = new TYP(server_ip, server_port, psize);

    sockaddr *addr;
    socklen_t addr_len;

    if(server->af == AF_INET){
        addr_len = sizeof(sockaddr_in);
        addr = (sockaddr *)malloc(addr_len);
    }else{
        addr_len = sizeof(sockaddr_in6);
        addr = (sockaddr *)malloc(addr_len);
    }

    pthread_t loop_thread;
    pthread_create(&loop_thread, NULL, [](void *ev_loop) -> void *{
        ((ev::default_loop *) ev_loop)->run(0);
        return NULL;
    }, &loop);

    while (true) {
        rv = server->listen();

        if(rv != 0){
            LOG(FATAL) << "Failed to bind: " << rv << " errno " << errno;
        }

        LOG(INFO) << "Listening on " << server_ip << ":" << server_port;

        rv = server->accpet(psk, addr, &addr_len);
        if(rv != 0){
            string ip = ip2str(addr);
            LOG(WARNING) << "Invalid handshake packet from " << ip << ", Code: " << rv;
            continue;
        }

        S5Server *tpserv = new S5Server(server, loop, psize, max_conn, conn_timeout);
        tpserv->run();

        LOG(WARNING) << "Loop finished , restarting";
    }
}
