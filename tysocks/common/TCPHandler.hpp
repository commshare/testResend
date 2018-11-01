//
//  TCPHandler.hpp
//  tysocks
//
//  Created by TYPCN on 2016/6/13.
//
//

#ifndef TCPHandler_hpp
#define TCPHandler_hpp

#include <stdio.h>
#include <ev++.h>
#include <list>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <typroto/spinlock.h>
#include "TYP.hpp"

#define sp 4

struct Buffer{
    uint8_t *data;
    int32_t len;
    int32_t wrote_len;
};

class TCPHandler {
private:
    ev::io io_read;
    ev::io io_write;
    int sfd;
    int conn_id;
    int packet_size;
    int Buffer_size;
    bool needSocks5Handshake;
    TYP *typroto;
    TCPHandler **conn_list; /*二维指针啊*/
    
    bool is_writeable;
    pthread_spinlock_t writeable_lock; /*这种锁*/
    
    std::list<Buffer *> write_queue; /*写入队列，里头都是buffer的指针呢*/
    pthread_mutex_t queue_lock;
    
    void write_cb(ev::io &watcher, int revents);
    void read_cb(ev::io &watcher, int revents);

    virtual ~TCPHandler() {
        io_read.stop();
        io_write.stop();
        close(sfd);
        /*清理写入队列啊*/
        for (std::list<Buffer *>::iterator it=write_queue.begin(); it != write_queue.end(); ++it){
            Buffer *b = *it;
            free(b->data); /*释放掉数据指针*/
            free(b); /*释放掉指针*/
        }
        uint8_t *data = (uint8_t *)malloc(4); /*4个字节，存放连接id*/
        memcpy(data, &conn_id, 4);
        typroto->send(data, 4); /*先发送4个字节的链接id*/
        pthread_spin_destroy(&writeable_lock);
        pthread_mutex_destroy(&queue_lock);
    }
    
public:
    TCPHandler(TCPHandler **conn_list_ptr, TYP *typ_handle, int s, int psize, int idx, bool needSocks5Handshake);
    void add_queue(const uint8_t *data, int len);
    void send(const uint8_t *data, int len);
    void shutdown();
};


#endif /* TCPHandler_hpp */
