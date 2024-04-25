#include "server.h"
#include <urcu.h>

int q_dictSdsKeyCaseMatch(struct cds_lfht_node *ht_node, const void *key) {
    struct q_dictEntry *de = caa_container_of(ht_node, struct q_dictEntry, node);
    return strcasecmp(de->key, key) == 0;
}

void q_freeDictEntry(q_dictEntry *de) {
    if (de == NULL) return;
    // free key which is sds string
    sdsfree(de->key);
    // free val which is shared
    if (de->v.val) {
        decrRefCount(de->v.val);
    }
    zfree(de);
}

void q_freeRcuDictEntry(struct rcu_head *head) {
    struct q_dictEntry *de = caa_container_of(head, struct q_dictEntry, rcu_head);
    q_freeDictEntry(de);
}

void q_freeDictExpirationEntry(q_dictEntry *de) {
    if (de == NULL) return;
    sdsfree(de->key);
    zfree(de);
}

void q_freeRcuDictExpirationEntry(struct rcu_head *head) {
    struct q_dictEntry *de = caa_container_of(head, struct q_dictEntry, rcu_head);
    q_freeDictExpirationEntry(de);
}

q_dictIterator *q_dictGetIterator(q_dict *ht) {
    q_dictIterator *iter = zmalloc(sizeof(*iter));
    iter->d = ht;
    cds_lfht_first(ht->table, &iter->iter);
    return iter;
}

void q_dictReleaseIterator(q_dictIterator* iter) {
    zfree(iter);
}


q_dictEntry *q_dictNext(q_dictIterator *iter) {
    struct cds_lfht_node *node;
    struct q_dictEntry *de = NULL;
    node = cds_lfht_iter_get_node(&iter->iter);
    if (node != NULL) {
        cds_lfht_next(iter->d->table, &iter->iter);
        de = caa_container_of(node, struct q_dictEntry, node);
        return de;
    }
    return NULL;
}

long long q_getExpire(redisDb *db, robj* key) {
    q_dictEntry *de;
    long long when;

    rcu_read_lock();
    // No expire
    if (q_dictSize(db->expires) == 0 ||
            (de = q_dictFind(db->expires, key->ptr)) == NULL) {
        rcu_read_unlock();
        return -1;
    }
    when = de->v.s64;
    rcu_read_unlock();
    //safety check
    serverAssertWithInfo(NULL,key,q_dictFind(db->dict,key->ptr) != NULL);

    return when; 
}

// Make sure this is called by worker thread only
// Make sure only read command call this function and write version
// call expireIfNeeded instead.
int q_expireIfNeeded(redisDb *db, robj* key) {
    mstime_t when = q_getExpire(db, key);
    mstime_t now;

    if (when < 0)  return 0; /*No expire for this key. */

    if(server.loading) return 0;

    now = server.lua_caller ? server.lua_time_start : mstime();

    if (server.masterhost != NULL) return now > when;

    if (now <= when) return 0;

    // we are the master and the key has been expired. 
    //ToDo: create a lock-free queue for server thread.
    // and add the key to queue for server thread processing
    
    //return 1;
    
    //ToDo: use above solution  
    //temp solution dbDelete is not thread safe for cluster mode
    return dbDelete(db, key);
}

// expire flag indicate whether q_dict is expiration dict
int q_dictDelete(q_dict *d, void *key, bool expire) {
    unsigned long hash;
    struct cds_lfht_node *ht_node;
    struct cds_lfht_iter iter;
    int ret = 0;
    int deleted = DICT_ERR;

    rcu_read_lock();
    hash = dictSdsHash(key);
    cds_lfht_lookup(d->table, hash, q_dictSdsKeyCaseMatch, key, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
    if (ht_node) {
        ret = cds_lfht_del(d->table, ht_node);
        if (ret) {
            // concurrently deleted.
        } else {
            q_dictEntry *de = caa_container_of(ht_node, struct q_dictEntry, node);
            if (!expire)
                call_rcu(&de->rcu_head, q_freeRcuDictEntry);
            else call_rcu(&de->rcu_head, q_freeRcuDictExpirationEntry);
            deleted = DICT_OK;
            --d->size;
        }
    } else {
        // not found node
    }
    rcu_read_unlock();
    return deleted;
}

q_dictEntry *q_createDictEntry(sds key, robj* val) {
    q_dictEntry *de = NULL;
    de = zmalloc(sizeof(*de));
    cds_lfht_node_init(&de->node);
    de->key = key;
    de->v.val = val;
    return de;
}

/* assume higher function has dup the sds key and 
 * has increment the val's reference. */
int q_dictAdd(q_dict *d, sds key, robj* val) {
    struct cds_lfht_node *ht_node;
    unsigned long hash;
    struct q_dictEntry *de;

    rcu_read_lock();
    de = q_createDictEntry(key, val);
    hash = dictSdsHash(key);
    ht_node = cds_lfht_add_replace(d->table, hash, q_dictSdsKeyCaseMatch, key, 
            &de->node);

    if (ht_node) {
        struct q_dictEntry *ode = caa_container_of(ht_node, struct q_dictEntry, node);
        call_rcu(&ode->rcu_head, q_freeRcuDictEntry);
        rcu_read_unlock();
        return DICT_REPLACED;
    } else {
        ++d->size;
        rcu_read_unlock();
        return DICT_OK;
    }

    return DICT_OK;
}

int q_dictAddExpiration(q_dict *d, sds key, long long when) {
    struct cds_lfht_node *ht_node;
    unsigned long hash;
    struct q_dictEntry *de;

    rcu_read_lock();
    de = zmalloc(sizeof(*de));
    cds_lfht_node_init(&de->node);
    de->key = key;
    de->v.s64 = when;
    hash = dictSdsHash(key);
    ht_node = cds_lfht_add_replace(d->table, hash, q_dictSdsKeyCaseMatch, key, 
            &de->node);
    if (ht_node) {
        struct q_dictEntry *ode = caa_container_of(ht_node, struct q_dictEntry, node);
        call_rcu(&ode->rcu_head, q_freeRcuDictExpirationEntry);
        rcu_read_unlock();
        return DICT_REPLACED;
    } else {
        ++d->size;
        rcu_read_unlock();
        return DICT_OK;
    }
    return DICT_OK;
}

/* called within rcu_read_lock held
 * in order to maintain the q_dictEntry valid until rcu_read_unlock is called.
 */
q_dictEntry* q_dictFind(q_dict *d, void *key) {
    unsigned long hash;
    struct cds_lfht_iter iter;
    struct cds_lfht_node *ht_node;

    hash = dictSdsHash(key);
    cds_lfht_lookup(d->table, hash, q_dictSdsKeyCaseMatch, key, &iter);
    ht_node = cds_lfht_iter_get_node(&iter);
    if (!ht_node) {
        return NULL;
    } else {
        // found
        q_dictEntry *de = caa_container_of(ht_node, struct q_dictEntry,node);
        return de;
    }
    return NULL;
}

void q_dictEmpty(q_dict *d, void(callback)(void*), bool expire) {
    unsigned long i = 0;
    struct cds_lfht_iter iter;
    struct cds_lfht_node *ht_node;
    struct q_dictEntry *entry;
    int ret = 0;

    rcu_read_lock();
    cds_lfht_for_each_entry(d->table, &iter, entry, node) {
        ht_node = cds_lfht_iter_get_node(&iter);
        ret = cds_lfht_del(d->table, ht_node);
        if (ret) {
            // concurrently delete
        } else {
            if (!expire) call_rcu(&entry->rcu_head, q_freeRcuDictEntry);
            else call_rcu(&entry->rcu_head, q_freeRcuDictExpirationEntry); // free expire dict entry
            ++i;
            if ( (i & 65535) == 0) callback(d->privdata);
        }
    }
    rcu_read_unlock();
    d->size = 0;
    return;
}

int q_dictGetRandomKeyMatch(struct cds_lfht_node *ht_node, const void *key)  {
    UNUSED(key);
    /*
     * if (ht_node->next == NULL) return 1;
     * if (ht_node->reverse_hash % 7 == 0) return 1;
     * else return 0;
     */
    return ht_node != NULL;
}
q_dictEntry *q_dictGetRandomKey(q_dict *d) 
{
    //ToDo: find a way to random access the hash table? 
    q_dictEntry *de = NULL;
    q_dictEntry *sde = NULL;
    struct cds_lfht_iter iter;
    //struct cds_lfht_node *ht_node = NULL;
    //unsigned long hash = random();
    //long key = 0;
    unsigned long size = 0;
    unsigned long count = 0;
    unsigned long i = 0;

    if (q_dictSize(d) == 0) return NULL;
    size = q_dictSize(d);
    if (size > 100000) size = 100000;
    count = random() % size;
    /*
     * cds_lfht does not expose bucket_at and size  
     * while (ht_node == NULL) {
     *     hash = random() % d->table->size;
     *     //cds_lfht_lookup(d->table, hash, q_dictGetRandomKeyMatch, &key, &iter);
     *     ht_node = d->table->bucket_at(d->table, hash); 
     * }
     */
    rcu_read_lock();
    cds_lfht_for_each_entry(d->table, &iter, de, node) {
        i++;
        sde = de;
        if (i == count) break;
    }
    rcu_read_unlock();
    return sde;
}

void q_dictGetStats(char *buf, size_t bufsize, q_dict *d) {

    snprintf(buf, bufsize,
            "Hash table stats:\n"
            " table size: %d\n",
            d->size);
    if (bufsize) buf[bufsize-1] = '\0';
    return;
}

