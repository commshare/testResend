//
//  DNS.hpp
//  tysocks
//
//  Created by TYPCN on 2016/6/16.
//
//

#ifndef DNS_hpp
#define DNS_hpp

#include <stdio.h>
#include <rdns.h>
#include <rdns_ev.h>
#include <functional>
#include <vector>
#include <string>
#include <unordered_map>

struct dns_result{
    int af;
    uint8_t *data;
};

typedef std::function<void(std::vector<dns_result *>)> dns_callback_t;

class DNS{
private:
    static DNS* dns_;
    struct rdns_resolver *resolver_ev;
    double timeout;
    int repeat;
    int qid;
    std::unordered_map<int, dns_callback_t> cb_map;
    std::unordered_map<std::string, std::vector<dns_result *>> dns_cache;
    
public:
    static DNS* get(){
        if (dns_ == NULL)
        {
            dns_ = new DNS;
        }
        return dns_;
    }
    void init(struct ev_loop *loop, double timeout_p, int repeat_p);
    void getAddrInfoAsync(int af,const char *name, dns_callback_t callback);
    void processResult(int id, struct rdns_reply *reply);
};

#endif /* DNS_hpp */
