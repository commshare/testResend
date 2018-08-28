//
//  S5Server.cpp
//  tysocks
//
//  Created by TYPCN on 2016/6/15.
//
//

#include "S5Server.hpp"
#include "Connector.hpp"
#include <logging.hpp>
#include <sodium.h>
#include <sockopt.hpp>

void S5Server::close_connection(int id){
    uint8_t *cdata = (uint8_t *)malloc(4);
    memcpy(cdata, &id, 4);
    tp_server->send(cdata, 4);
}

void S5Server::process_control_packet(const uint8_t *buf, uint32_t len){
    if(buf[0] == 'C' && buf[1] == 'F' && buf[2] == 'G'){
        apply_sock_opt(buf, tp_server->raw());
        print_sock_opt("from client", tp_server->raw());
    }
}

void S5Server::on_data(int fd, const uint8_t *buf, uint32_t len, const struct sockaddr* addr, socklen_t addrlen){
    int conn_id = *(int *)buf;
    if(conn_id < 0) return this->process_control_packet(buf, len);
    
    TCPHandler *hdl = conn_list[conn_id];

    // 过段时间重构这破代码。。
    // SP 为 Start Position，则跳过连接 ID （ 前面4个字节 ）
    if(len == sp){
        if(hdl){
            hdl->shutdown();
        }
    }else if(hdl){
        hdl->send(buf + sp, len - sp);
    }else if(buf[sp] == 0x05){ // SOCKS5 Handshake
        uint8_t cmd = buf[sp + 1];
        if(cmd != 0x01){
            this->close_connection(conn_id);
            return;
        }
        Connector *conn = new Connector(this, conn_id);
        uint8_t atype = buf[sp + 3]; // 该字节为连接类型， 1 为 IPv4 地址，3 为域名，4 为 IPv6 地址
        if(atype == 0x01){ // IPv4
            const uint8_t *addrptr = buf + sp + 4; // 4 个字节 V4 地址
            uint16_t port = *(uint16_t *)(buf + sp + 8); // 两个字节端口（网络端序）
            conn->connectV4(addrptr,  port);
        }else if(atype == 0x03){ // Domain
            uint8_t domainlen = *(uint8_t *)(buf + sp + 4); // DSTADDR 第一个字节为域名长度
            const uint8_t *domainptr = buf + sp + 5; // 域名内容，从 DSTADDR 第二个字节开始
            const uint8_t *portptr = buf + sp + 5 + domainlen; // 两个字节端口（网络端序）
            conn->connectDomain(domainptr, domainlen, *(uint16_t *)portptr);
        }else if(atype == 0x04){ // IPv6
            const  uint8_t *addrptr = buf + sp + 4; // 16 个字节 V6 地址
            uint16_t port = *(uint16_t *)(buf + sp + 20); // 两个字节端口（网络端序）
            conn->connectV6(addrptr,  port);
        }else{
            delete conn;
            this->close_connection(conn_id);
            string ip = ip2str(addr);
            LOG(INFO) << "Received packet with invalid addr type from " << ip;
        }
    }else if(len > 4){
        string ip = ip2str(addr);
        LOG(WARNING) << "Received packet with invalid id " << conn_id << " from " << ip;
    }
}

void S5Server::on_event(int fd, int code, const char* msg, const struct sockaddr* addr, socklen_t addrlen){
    if(code == -2002){
        if(!__sync_bool_compare_and_swap(&stop_loop, false, true)){
            return;
        }
        hb_timer.stop();
        running_cv.notify_all();
        LOG(ERROR) << "Remote not responding , stopping.";
    }else{
        string ip = ip2str(addr);
        LOG(WARNING) << "Event: " << msg << " Code: " << code << " IP: " << ip;
    }
}

void S5Server::heartbeat(ev::timer &timer, int revents){
    uint8_t *cdata = (uint8_t *)malloc(4);
    memset(cdata, 0xFF, 4);
    tp_server->send(cdata, 4);
    hb_timer.repeat = 1 + randombytes_uniform(10);
    hb_timer.again();
}

void S5Server::run(){
    hb_timer.set<S5Server, &S5Server::heartbeat>(this);
    hb_timer.start(1 + randombytes_uniform(10));
    while(!stop_loop){
        std::unique_lock<std::mutex> lk(running_mtx);
        running_cv.wait(lk);
    }
    tp_server->restart();
    for (int i = 0 ; i < max_conn; i++) {
        if(conn_list[i]){
            conn_list[i]->shutdown();
        }
    }
    free(conn_list);
    delete this;
}

S5Server::S5Server(TYP* server, ev::loop_ref loop, int psize, int max_conn, int conn_timeout) : loop_ref(loop), max_conn(max_conn) , pktSize(psize), timeout_ms(conn_timeout) , tp_server(server)  {
    server->setRecvCallback<S5Server, &S5Server::on_data>(this);
    server->setEventCallback<S5Server, &S5Server::on_event>(this);
    
    stop_loop = false;
    int thsize = sizeof(TCPHandler *);
    //LOG(INFO) << "sizeof(TCPHandler *) = " << thsize << " max_conn = " << max_conn;
    conn_list = (TCPHandler **)malloc(thsize * max_conn);
}
