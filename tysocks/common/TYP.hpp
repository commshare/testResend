//
//  TYP.hpp
//  tysocks
//
//  Created by TYPCN on 2016/6/12.
//
//

#ifndef typ_hpp
#define typ_hpp

#include <string>
#include <stdint.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
#include <typroto/typroto.h>
#include <typroto/socks.h>
#include "utils.hpp"

using namespace std;

class TYP {
private:
    int fd;
    int packet_size;
    struct sockaddr *addr;
    socklen_t addrlen;

public:
    int af;
    
    TYP(string ip, int port, int psize) {
        addr = NULL;
        af = str2ip(ip, port, &addr);
        fd = tp_socket(af);
        packet_size = psize;
        if(af == AF_INET){
            addrlen = sizeof(sockaddr_in);
        }else{
            addrlen = sizeof(sockaddr_in6);
        }
    }
    
    int listen(){
        return tp_listen(fd, addr, addrlen);
    }
    
    int accpet(string key, struct sockaddr *addr_i, socklen_t *addrlen_i){
        return tp_accept(fd, packet_size, key.c_str(), addr_i, addrlen_i);
    }
    
    int connect(string key) {
        return tp_connect(fd, packet_size, key.c_str(), addr, addrlen);
    }
    
    int send(uint8_t *buf, size_t len){
        return tp_send(fd, buf, len);
    }
    
    template<class K, void (K::*method)(int, const uint8_t *, uint32_t, const struct sockaddr*, socklen_t)>
    void setRecvCallback (K *object)
    {
        tp_set_recv_callback(fd, recv_thunk<K, method> , object);
    }
    
    template<class K, void (K::*method)(int, const uint8_t *, uint32_t, const struct sockaddr*, socklen_t)>
    static void recv_thunk (int fd, const uint8_t *buf, uint32_t len, const struct sockaddr* addr, socklen_t addrlen, void *userdata)
    {
        (static_cast<K *>(userdata)->*method)(fd,buf,len,addr,addrlen);
    }
    
    
    template<class K, void (K::*method)(int, int, const char*,const struct sockaddr*, socklen_t)>
    void setEventCallback (K *object)
    {
        tp_set_event_callback(fd, evt_thunk<K, method> , object);
    }
    
    template<class K, void (K::*method)(int, int, const char*,const struct sockaddr*, socklen_t)>
    static void evt_thunk (int fd, int code, const char* msg,const struct sockaddr* addr, socklen_t addrlen, void *userdata)
    {
        (static_cast<K *>(userdata)->*method)(fd,code,msg,addr,addrlen);
    }
    
    
    const struct Socket *raw(){
        return tp_get_sock_by_id(fd);
    }
    
    void restart(){
        tp_shutdown(fd);
        sleep(1);
        fd = tp_socket(af);
        int enable = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *) (void *) &enable, sizeof(int));
    }
    
    virtual ~TYP() {
        tp_shutdown(fd);
        free(addr);
    }
};

#endif /* typ_hpp */
