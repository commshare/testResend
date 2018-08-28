#include <ev++.h>
#include <string>
#include <iostream>
#include <typroto/typroto.h>
#include <logging.hpp>
#include <INIReader.hpp>
#include <TYP.hpp>
#include <sockopt.hpp>
#include "TCPServer.hpp"

using namespace std;

CREATE_LOGGER;

int main(int argc, char *argv[]){
    INIT_LOGGER;
    string cfgpath = "/etc/tysocks/client";
    if (argc > 1)
        cfgpath = argv[1];

    INIReader reader(cfgpath);
    if (reader.ParseError() < 0) {
        cout << "Usage: ./tysocks_client <config file path>" << endl;
        return 1;
    }

    #ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
    #endif
    // Setup typroto
    int rv = tp_init();
    if(rv != 0){
        LOG(FATAL) << "Failed to init typroto";
    }

    // Setup libev
    setevbackend(reader.Get("TCP", "backend", "select"));
    ev::default_loop loop;

    // Remote config
    string remote_ip = reader.Get("Remote", "addr", "127.0.0.1");
    string psk = reader.Get("Remote", "key", "");
    int remote_port = reader.GetInteger("Remote", "port", 443);
    int psize = reader.GetInteger("Remote", "pktSize", 1400);

    // Local config
    string local_ip = reader.Get("Local", "bind", "127.0.0.1");
    int local_port = reader.GetInteger("Local", "port", 1080);

    // TCP config
    int max_queue = reader.GetInteger("TCP", "queue", 128);
    int max_conn = reader.GetInteger("TCP", "maxconn", 8192);

    // Proto config
    uint8_t *client_sock_opt = (uint8_t *)malloc(32);
    pack_sock_opt("LocalSockOpt", reader, client_sock_opt);
    uint8_t *server_sock_opt = (uint8_t *)malloc(32);
    pack_sock_opt("RemoteSockOpt", reader, server_sock_opt);

    // Create typroto connection
    TYP *client = new TYP(remote_ip, remote_port, psize);

    LOG(INFO) << "Connecting to " << remote_ip << ":" << remote_port;

    rv = client->connect(psk);

    if(rv != 0){
        LOG(FATAL) << "Failed to connect: " << rv;
    }

    apply_sock_opt(client_sock_opt, client->raw());
    free(client_sock_opt);

    client->send(server_sock_opt, 32);

    // Create TCP Server and listen
    TCPServer serv(client, local_ip, local_port, max_queue, psize, max_conn);

    LOG(INFO) << "Listening on " << local_ip << ":" << local_port;

    loop.run(0);
}
