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