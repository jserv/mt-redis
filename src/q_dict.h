//
// Created by chunlei zhang on 2018/11/21.
//
//
#ifndef __Q_DICT_H__
#define __Q_DICT_H__

#include "sds.h"
#include <urcu.h>
#include <urcu/rculfhash.h>

#define q_dictSize(d) ((d)->size)

typedef struct q_dictEntry {
    unsigned type:4;   // four data structure types: string, list, set, zset and hash
    void *key;
    union {
        void *val;
        int64_t s64;
    } v;
    struct cds_lfht_node node;
    struct rcu_head rcu_head;
} q_dictEntry;

typedef struct q_dict {
    unsigned int size;
    struct cds_lfht *table;
    void *privdata;
} q_dict;

typedef struct q_dictIterator {
    q_dict *d;
    struct cds_lfht_iter iter;
} q_dictIterator;

struct redisDb;
struct redisObject;  //alias robj defined in server.h

unsigned int dictSdsHash(const void *key);
q_dictIterator *q_dictGetIterator(q_dict *d);
void q_dictReleaseIterator(q_dictIterator* iter);
q_dictEntry *q_dictNext(q_dictIterator *iter);
int q_dictDelete(q_dict *d, void *key, bool expire);
q_dictEntry* q_dictFind(q_dict *d, void *key);
int q_dictAddExpiration(q_dict *d, sds key, long long when);
int q_dictAdd(q_dict *d, sds key, struct redisObject* val);
q_dictEntry *q_createDictEntry(sds key, struct redisObject* val);
int q_expireIfNeeded(struct redisDb *db, struct redisObject* key);
long long q_getExpire(struct redisDb *db, struct redisObject* key);
void q_freeDictExpirationEntry(q_dictEntry *de);
void q_freeRcuDictExpirationEntry(struct rcu_head *head);
void q_freeRcuDictEntry(struct rcu_head *head);
void q_freeDictEntry(q_dictEntry *de);
int q_dictSdsKeyCaseMatch(struct cds_lfht_node *ht_node, const void *key);
void q_dictEmpty(q_dict *d, void(callback)(void*),bool expire);
q_dictEntry *q_dictGetRandomKey(q_dict *d);
void q_dictGetStats(char *buf, size_t bufsize, q_dict *d);

#endif 
