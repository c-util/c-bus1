#pragma once

/*
 * Internal Definitions
 */

#include <c-list.h>
#include <c-rbtree.h>
#include <c-stdaux.h>
#include <limits.h>
#include <stdlib.h>
#include "bus1.h"
#include "c-bus1.h"

/* Maximum number of free events that are cached for re-use. */
#define C_BUS1_EVENT_CACHE_SIZE (16UL)

struct CBus1Event {
        unsigned long n_refs;
        CList link_b1;
        CBus1 *b1;

        unsigned int type;
        bool used : 1;

        union {
                uint64_t id;
                CBus1Object *object;
                CBus1Handle *handle;
        } destination;

        union {
                CBus1Message user;
                struct bus1_message kernel;
        } message;
};

#define C_BUS1_EVENT_NULL(_x) {                                                 \
                .n_refs = 1,                                                    \
                .link_b1 = C_LIST_INIT((_x).link_b1),                           \
                .type = _C_BUS1_EVENT_TYPE_N,                                   \
        }

struct CBus1Handle {
        unsigned long n_refs;
        CRBNode rb_b1;
        uint64_t id;
        CBus1 *b1;
};

#define C_BUS1_HANDLE_NULL(_x) {                                                \
                .n_refs = 1,                                                    \
                .rb_b1 = C_RBNODE_INIT((_x).rb_b1),                             \
                .id = BUS1_INVALID,                                             \
        }

struct CBus1Object {
        unsigned long n_refs;
        CRBNode rb_b1;
        uint64_t id;
        CBus1 *b1;
        void *userdata;
        bool destroyed : 1;
        bool released : 1;
};

#define C_BUS1_OBJECT_NULL(_x) {                                                \
                .n_refs = 1,                                                    \
                .rb_b1 = C_RBNODE_INIT((_x).rb_b1),                             \
                .id = BUS1_INVALID,                                             \
        }

struct CBus1 {
        int fd;
        CRBTree map_objects;
        CRBTree map_handles;
        CList unused_events;
        CList incoming_events;
        CList live_events;
        size_t n_unused_events;
};

#define C_BUS1_NULL(_x) {                                                       \
                .fd = -1,                                                       \
                .map_objects = C_RBTREE_INIT,                                   \
                .map_handles = C_RBTREE_INIT,                                   \
                .unused_events = C_LIST_INIT((_x).unused_events),               \
                .incoming_events = C_LIST_INIT((_x).incoming_events),           \
                .live_events = C_LIST_INIT((_x).live_events),                   \
        }

/* objects */

CBus1Object *c_bus1_object_ref(CBus1Object *object);
CBus1Object *c_bus1_object_unref(CBus1Object *object);

/* contexts */

CBus1Object *c_bus1_find_raw_object(CBus1 *b1, uint64_t id);
CBus1Handle *c_bus1_find_raw_handle(CBus1 *b1, uint64_t id);

/* inline helpers */

static inline void c_bus1_object_unrefp(CBus1Object **object) {
        if (*object)
                c_bus1_object_unref(*object);
}
