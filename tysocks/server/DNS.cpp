//
//  DNS.cpp
//  tysocks
//
//  Created by TYPCN on 2016/6/16.
//
//

#include "DNS.hpp"
#include <assert.h>
#include <logging.hpp>

#ifndef INT32_MAX
#define INT32_MAX 2147483647
#endif

void DNS::init(struct ev_loop *loop, double timeout_p, int repeat_p){
    resolver_ev = rdns_resolver_new();
    rdns_bind_libev(resolver_ev, loop);
    rdns_resolver_parse_resolv_conf(resolver_ev, "/etc/resolv.conf");
    rdns_resolver_init(resolver_ev);
    timeout = timeout_p;
    repeat = repeat_p;
    qid = 0;
}

static void dns_callback (struct rdns_reply *reply, void *arg)
{
    DNS *dns = DNS::get();
    int query_id = *(int *)arg;
    dns->processResult(query_id, reply);
    free(arg);
}

void DNS::processResult(int id, struct rdns_reply *reply){
    dns_callback_t cb = cb_map[id];
    if(!cb){
        LOG(ERROR) << "No callback for dns query id " << id;
        return;
    }
    struct rdns_reply_entry *entry;

    std::vector<dns_result *> addrs;

    int dr_size = sizeof(dns_result);

    const struct rdns_request_name *name;
    name = rdns_request_get_name(reply->request, NULL);

    if (reply->code == RDNS_RC_NOERROR) {
        entry = reply->entries;
        while (entry != NULL) {
            if (entry->type == RDNS_REQUEST_A) {
                dns_result *result = (dns_result *)malloc(dr_size);
                result->af = AF_INET;
                result->data = (uint8_t *)malloc(4);
                memcpy(result->data, &entry->content.a.addr.s_addr, 4);
                addrs.push_back(result);
            }else if (entry->type == RDNS_REQUEST_AAAA) {
                dns_result *result = (dns_result *)malloc(dr_size);
                result->af = AF_INET6;
                result->data = (uint8_t *)malloc(16);
                memcpy(result->data, &entry->content.aaa.addr.s6_addr, 16);
                addrs.push_back(result);
            }
            entry = entry->next;
        }
    }else{
        LOG(INFO) << "DNS failed for " << name->name << " type " << rdns_strtype(name->type) << ": " << rdns_strerror (reply->code);
    }
    
    cb(addrs);
    
    dns_cache[name->name] = addrs;
}

void DNS::getAddrInfoAsync(int af, const char *name, dns_callback_t callback){
    if(dns_cache.count(name) > 0){
        callback(dns_cache[name]);
        return;
    }else if(qid+1 == INT32_MAX){
        qid = 0;
    }
    qid++;
    int query_id = time(NULL) + qid;
    void *intptr = malloc(4);
    memcpy(intptr, &query_id, 4);
    struct rdns_request *req = nullptr;
    if(af == AF_INET){
        req = rdns_make_request_full(resolver_ev, dns_callback, intptr, timeout, repeat, 1, name, RDNS_REQUEST_A);
    }else if(af == AF_INET6){
        req = rdns_make_request_full(resolver_ev, dns_callback, intptr, timeout, repeat, 1, name, RDNS_REQUEST_AAAA);
    }
    if(!req){
        LOG(ERROR) << "Unable to query DNS name " << name;
    }
    cb_map[query_id] = callback;
}