//
//  TCPHandler.cpp
//  tysocks
//
//  Created by TYPCN on 2016/6/13.
//
//

#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define SHUT_RDWR SD_BOTH
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <fcntl.h>
#endif
#include <errno.h>
#include <typroto/typroto.h>
#include "TCPHandler.hpp"
#include <logging.hpp>

TCPHandler::TCPHandler(TCPHandler **conn_list_ptr, TYP *typ_handle, int s, int psize, int idx, bool needSocks5Handshake) : sfd(s) , conn_id(idx), packet_size(psize), needSocks5Handshake(needSocks5Handshake) , typroto(typ_handle) , conn_list(conn_list_ptr) {
    
    is_writeable = true;
initmtxlock:
    int rv = pthread_mutex_init(&queue_lock,  NULL);
    if(rv != 0){
        if(errno == EAGAIN){
            goto initmtxlock;
        }
        LOG(FATAL) << "Unable to init mutex lock with errno " << errno;
    }
initspinlock:
    rv = pthread_spin_init(&writeable_lock, PTHREAD_PROCESS_PRIVATE);
    if(rv != 0){
        if(errno == EAGAIN){
            goto initspinlock;
        }
        LOG(FATAL) << "Unable to init spin lock with errno " << errno;
    }
    
    set_non_blocking(s);
    io_read.set<TCPHandler, &TCPHandler::read_cb>(this);
    io_read.start(s, ev::READ);
    io_write.set<TCPHandler, &TCPHandler::write_cb>(this);

    Buffer_size = sizeof(Buffer);
}

void TCPHandler::read_cb(ev::io &watcher, int revents) {
    int buflen = packet_size - PROTOCOL_ABYTE;
    uint8_t *data = (uint8_t *)malloc(buflen);

    ssize_t nread = recv(watcher.fd, (char *) data + 4, buflen - 4, 0);
    
    if(nread < 0){
        free(data);
        LOG(ERROR) << "Read fd " << watcher.fd << " failed with errno " << errno;
        return;
    }else if(nread == 0){
        free(data);
        LOG(INFO) << "Closing connection " << watcher.fd;
        this->shutdown();
    }else if(needSocks5Handshake && data[sp] == 0x05){
        uint8_t res[2];
        res[0] = 0x05;
        res[1] = 0xFF;
        int methodLen = data[sp + 1];
        int accept = 0;
        for (int i = 0; i < methodLen; i++) {
            if(data[sp + 2 + i] == 0x00){
                accept = 1;
                res[1] = 0x00;
                break;
            }
        }
        needSocks5Handshake = false;
        this->send(res, 2);
        free(data);
        if(!accept){
            LOG(INFO) << "Closing connection " << watcher.fd << " because invalid handshake";
            this->shutdown();
        }
    }else{
        memcpy(data, &conn_id, 4);
        typroto->send((uint8_t *)data, nread + 4);
    }
}

void TCPHandler::write_cb(ev::io &watcher, int revents) {
    pthread_mutex_lock(&queue_lock);

    while (!write_queue.empty()) {
        Buffer* buffer = write_queue.front();
        
        ssize_t written = ::send(sfd, (const char *) buffer->data + buffer->wrote_len, buffer->len - buffer->wrote_len, 0);
        if (written < 0) {
            LOG(ERROR) << "Send fd " << watcher.fd << " failed with errno " << errno;
            return;
        }
        
        buffer->wrote_len += written;
        
        if (buffer->wrote_len != buffer->len) {
            pthread_mutex_unlock(&queue_lock);
            return;
        }
        
        write_queue.pop_front();
        free(buffer->data);
        free(buffer);
    }

    pthread_mutex_unlock(&queue_lock);
    io_write.stop();
    pthread_spin_lock(&writeable_lock);
    is_writeable = true;
    pthread_spin_unlock(&writeable_lock);
}


void TCPHandler::send(const uint8_t *data, int len){
    if(!this){
        return;
    }
    pthread_spin_lock(&writeable_lock);
    bool writeable = is_writeable;
    pthread_spin_unlock(&writeable_lock);
    if(writeable){
        // 可写直接发送
        ssize_t written = ::send(sfd, (const char *) data, len, 0);
        if(written < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                // Buffer 满了，设置为不可写
                pthread_spin_lock(&writeable_lock);
                is_writeable = false;
                pthread_spin_unlock(&writeable_lock);
                // 加入队列并等待可写事件回调
                this->add_queue(data, len);
                io_write.start(sfd, ev::WRITE);
            }else{
                LOG(ERROR) << "Send fd " << sfd << " failed with error " << errno;
                this->shutdown();
            }
        }else if(written < len){
            // 只发了一部分
            pthread_spin_lock(&writeable_lock);
            is_writeable = false;
            pthread_spin_unlock(&writeable_lock);
            this->add_queue(data + written, len - written);
            io_write.start(sfd, ev::WRITE);
        }
    }else{
        this->add_queue(data, len);
    }
}

void TCPHandler::add_queue(const uint8_t *data, int len){
    pthread_mutex_lock(&queue_lock);
    void *mem = malloc(len);
    memcpy(mem, data, len);
    Buffer *buf = (Buffer *)malloc(Buffer_size);
    buf->data = (uint8_t *)mem;
    buf->wrote_len = 0;
    buf->len = len;
    write_queue.push_back(buf);
    pthread_mutex_unlock(&queue_lock);
}

void TCPHandler::shutdown(){
    if(!this || !conn_list || !conn_list[conn_id]){
        return;
    }
    if(!__sync_bool_compare_and_swap(&conn_list[conn_id], this, NULL)){
        return;
    }
    ::shutdown(sfd, SHUT_RDWR);
    delete this;
}
