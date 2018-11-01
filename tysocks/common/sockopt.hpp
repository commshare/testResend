//
//  sockopt.hpp
//  tysocks
//
//  Created by TYPCN on 2016/6/17.
//
//

#ifndef sockopt_h
#define sockopt_h

#ifdef __INIREADER_H__

inline static int pack_sock_opt(string type, INIReader &reader, uint8_t *data){
    int winsize = reader.GetInteger(type, "windowSize", 500);
    int rsnddelay = reader.GetInteger(type, "resendDelay", 250);
    int speedlimit = reader.GetInteger(type, "maxPPS", 50000);
    int ackdelay = reader.GetInteger(type, "ACKDelay", 100);
    int sackdelay = reader.GetInteger(type, "SACKDelay", 100);
    int sackthreshold = reader.GetInteger(type, "SACKThreshold", 10);
    int resendTimeout = reader.GetInteger(type, "resendTimeout", 1000);
    data[0] = 'C';
    data[1] = 'F';
    data[2] = 'G';
    data[3] = 0xFF;
    memcpy(data + 4, &winsize, 4);
    memcpy(data + 8, &rsnddelay, 4);
    memcpy(data + 12, &speedlimit, 4);
    memcpy(data + 16, &ackdelay, 4);
    memcpy(data + 20, &sackdelay, 4);
    memcpy(data + 24, &sackthreshold, 4);
    memcpy(data + 28, &resendTimeout, 4);
    return 32;
}

#endif

inline static void apply_sock_opt(const uint8_t *data, const struct Socket* skt){
    int winsize = *(int *)(data + 4);
    int rsnddelay = *(int *)(data + 8);
    int speedlimit = *(int *)(data + 12);
    int ackdelay = *(int *)(data + 16);
    int sackdelay = *(int *)(data + 20);
    int sackthreshold = *(int *)(data + 24);
    int resendTimeout = *(int *)(data + 28);
    skt->sendqueue->window_size = winsize;
    skt->sendqueue->resend_delay = rsnddelay;
    skt->sendqueue->speed_limit = speedlimit;
    skt->sendqueue->max_resend_timeout = resendTimeout;
    skt->recvqueue->ack_send_delay = ackdelay;
    skt->recvqueue->sack_send_delay = sackdelay;
    skt->recvqueue->sack_threshold = sackthreshold;
}

/*发送队列的窗口、重传延迟、最大pps和最大重传。
 * 接收队列的 ack 延迟。sak延迟。sack的阈值*/

/*
 * TCP接收端提供SACK功能，通过头部累计ACK号字段来描述收到的数据。
ACK号与接收端缓存中其他数据之间的间隔成为空缺。Seq高于空缺数据成为失序数据。
每个SACK信息包括4字节开始位置，4字节结束为止（表示收到数据的起始值最后一个序列号+1），
 2字节填充。因此一个SACK为10个字节。SACK还会与TSOPT一起使用，又用掉10字节。
所以每个ACK中只能包含3个SACK块。（选项一共40字节）。
第一个SACK块为最新收到的数据，也就是最后的那段，其余两端为从头开始收到的两端连续数据。因此这就有两个空洞了

 * */


inline static void print_sock_opt(std::string name, const struct Socket* skt){
    LOG(INFO) << "Socket options " << name << ":\n"
              << "Window Size: " << skt->sendqueue->window_size << " "
              << "Resend Delay: " << skt->sendqueue->resend_delay << " "
              << "Max PPS: " << skt->sendqueue->speed_limit << " "
              << "Max resend: " << skt->sendqueue->max_resend_timeout << "\n"
              << "ACK Delay: " << skt->recvqueue->ack_send_delay << " "
              << "SACK Delay: " << skt->recvqueue->sack_send_delay << " "
              << "SACK Threshold: " << skt->recvqueue->sack_threshold << " ";
}


#endif /* sockopt_h */