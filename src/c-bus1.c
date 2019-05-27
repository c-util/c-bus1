/*
 * Bus1 Bindings
 */

#include <assert.h>
#include <c-list.h>
#include <c-rbtree.h>
#include <c-stdaux.h>
#include <stdlib.h>
#include <unistd.h>
#include "bus1.h"
#include "c-bus1.h"
#include "c-bus1-private.h"

/* verify CBus1Transfer */
static_assert(sizeof(((struct bus1_transfer){})) == sizeof(((CBus1Transfer){})), "ABI Mismatch");
static_assert(alignof(((struct bus1_transfer){})) == alignof(((CBus1Transfer){})), "ABI Mismatch");
static_assert(sizeof(((struct bus1_transfer){}).flags) == sizeof(((CBus1Transfer){}).flags), "ABI Mismatch");
static_assert(offsetof(struct bus1_transfer, flags) == offsetof(CBus1Transfer, flags), "ABI Mismatch");
static_assert(sizeof(((struct bus1_transfer){}).id) == sizeof(((CBus1Transfer){}).id), "ABI Mismatch");
static_assert(offsetof(struct bus1_transfer, id) == offsetof(CBus1Transfer, id), "ABI Mismatch");
static_assert(sizeof(((struct bus1_transfer){}).id) == sizeof(((CBus1Transfer){}).id.u64), "ABI Mismatch");
static_assert(offsetof(struct bus1_transfer, id) == offsetof(CBus1Transfer, id.u64), "ABI Mismatch");

/* verify CBus1Message */
static_assert(sizeof(((struct bus1_message){})) == sizeof(((CBus1Message){})), "ABI Mismatch");
static_assert(alignof(((struct bus1_message){})) == alignof(((CBus1Message){})), "ABI Mismatch");
static_assert(sizeof(((struct bus1_message){}).flags) == sizeof(((CBus1Message){}).flags), "ABI Mismatch");
static_assert(offsetof(struct bus1_message, flags) == offsetof(CBus1Message, flags), "ABI Mismatch");
static_assert(sizeof(((struct bus1_message){}).type) == sizeof(((CBus1Message){}).type), "ABI Mismatch");
static_assert(offsetof(struct bus1_message, type) == offsetof(CBus1Message, type), "ABI Mismatch");
static_assert(sizeof(((struct bus1_message){}).n_transfers) == sizeof(((CBus1Message){}).n_transfers), "ABI Mismatch");
static_assert(offsetof(struct bus1_message, n_transfers) == offsetof(CBus1Message, n_transfers), "ABI Mismatch");
static_assert(sizeof(((struct bus1_message){}).ptr_transfers) == sizeof(((CBus1Message){}).ptr_transfers), "ABI Mismatch");
static_assert(offsetof(struct bus1_message, ptr_transfers) == offsetof(CBus1Message, ptr_transfers), "ABI Mismatch");
static_assert(sizeof(((struct bus1_message){}).ptr_transfers) == sizeof(((CBus1Message){}).ptr_transfers.u64), "ABI Mismatch");
static_assert(offsetof(struct bus1_message, ptr_transfers) == offsetof(CBus1Message, ptr_transfers.u64), "ABI Mismatch");
static_assert(sizeof(((struct bus1_message){}).n_data) == sizeof(((CBus1Message){}).n_data), "ABI Mismatch");
static_assert(offsetof(struct bus1_message, n_data) == offsetof(CBus1Message, n_data), "ABI Mismatch");
static_assert(sizeof(((struct bus1_message){}).n_data_vecs) == sizeof(((CBus1Message){}).n_data_vecs), "ABI Mismatch");
static_assert(offsetof(struct bus1_message, n_data_vecs) == offsetof(CBus1Message, n_data_vecs), "ABI Mismatch");
static_assert(sizeof(((struct bus1_message){}).ptr_data_vecs) == sizeof(((CBus1Message){}).ptr_data_vecs), "ABI Mismatch");
static_assert(offsetof(struct bus1_message, ptr_data_vecs) == offsetof(CBus1Message, ptr_data_vecs), "ABI Mismatch");
static_assert(sizeof(((struct bus1_message){}).ptr_data_vecs) == sizeof(((CBus1Message){}).ptr_data_vecs.u64), "ABI Mismatch");
static_assert(offsetof(struct bus1_message, ptr_data_vecs) == offsetof(CBus1Message, ptr_data_vecs.u64), "ABI Mismatch");

static void c_bus1_event_bind(CBus1Event *event, CBus1 *b1) {
        c_assert(!event->b1);
        c_assert(!event->used);

        event->b1 = b1;
        c_list_link_front(&b1->unused_events, &event->link_b1);
        ++b1->n_unused_events;
}

static void c_bus1_event_unbind(CBus1Event *event, bool release) {
        size_t i;
        int r;

        if (!event->b1)
                return;

        if (release) {
                switch (event->type) {
                case C_BUS1_EVENT_TYPE_KERNEL:
                case C_BUS1_EVENT_TYPE_RELEASE:
                case C_BUS1_EVENT_TYPE_DESTRUCTION: {
                        struct bus1_transfer *transfers;
                        struct bus1_message *message;

                        message = &event->message.kernel;
                        transfers = (void *)(unsigned long)message->ptr_transfers;
                        for (i = 0; i < event->message.kernel.n_transfers; ++i) {
                                r = c_bus1_sys_release(event->b1->fd,
                                                       0,
                                                       1,
                                                       (uint64_t *)&transfers[i].id);
                                c_assert(!r);
                        }

                        break;
                }

                case C_BUS1_EVENT_TYPE_MESSAGE: {
                        /* nothing to do */
                        break;
                }

                default:
                        c_assert(!event->used);
                        break;
                }
        }

        if (!event->used)
                --event->b1->n_unused_events;

        c_list_unlink(&event->link_b1);
        event->b1 = NULL;
}

static void c_bus1_event_claim(CBus1Event *event) {
        c_assert(event->b1);
        c_assert(!event->used);

        c_list_unlink_stale(&event->link_b1);
        c_list_link_tail(&event->b1->incoming_events, &event->link_b1);
        --event->b1->n_unused_events;
        event->used = true;
}

static void c_bus1_event_publish(CBus1Event *event) {
        c_assert(event->b1);
        c_assert(event->used);

        c_list_unlink_stale(&event->link_b1);
        c_list_link_tail(&event->b1->live_events, &event->link_b1);
}

static void c_bus1_event_init(CBus1Event *event) {
        *event = (CBus1Event)C_BUS1_EVENT_NULL(*event);
}

static void c_bus1_event_deinit(CBus1Event *event) {
        size_t i;

        c_assert(!event->n_refs);

        c_bus1_event_unbind(event, true);

        switch (event->type) {
        case C_BUS1_EVENT_TYPE_KERNEL: {
                /* nothing to do */
                break;
        }

        case C_BUS1_EVENT_TYPE_MESSAGE: {
                CBus1Transfer *transfers;

                c_bus1_object_unref(event->destination.object);

                transfers = event->message.user.ptr_transfers.ptr;
                for (i = 0; i < event->message.user.n_transfers; ++i)
                        c_bus1_handle_unref(transfers[i].id.handle);

                break;
        }

        case C_BUS1_EVENT_TYPE_RELEASE: {
                c_bus1_object_unref(event->destination.object);
                break;
        }

        case C_BUS1_EVENT_TYPE_DESTRUCTION: {
                c_bus1_handle_unref(event->destination.handle);
                break;
        }

        default:
                c_assert(!event->used);
                break;
        }
}

static void c_bus1_event_recycle(CBus1Event *event, CBus1 *b1) {
        if (b1 && b1->n_unused_events < C_BUS1_EVENT_CACHE_SIZE) {
                c_bus1_event_init(event);
                c_bus1_event_bind(event, b1);
        } else {
                free(event);
        }
}

static int c_bus1_event_new(CBus1Event **eventp) {
        _c_cleanup_(c_bus1_event_unrefp) CBus1Event *event = NULL;

        event = calloc(1, sizeof(*event));
        if (!event)
                return -ENOMEM;

        c_bus1_event_init(event);

        *eventp = event;
        event = NULL;
        return 0;
}

/**
 * c_bus1_event_ref() - XXX
 */
_c_public_ CBus1Event *c_bus1_event_ref(CBus1Event *event) {
        if (event)
                ++event->n_refs;
        return event;
}

/**
 * c_bus1_event_unref() - XXX
 */
_c_public_ CBus1Event *c_bus1_event_unref(CBus1Event *event) {
        CBus1 *b1;

        if (!event || --event->n_refs)
                return NULL;

        b1 = event->b1;
        c_bus1_event_deinit(event);
        c_bus1_event_recycle(event, b1);

        return NULL;
}

/**
 * c_bus1_event_get_type() - XXX
 */
_c_public_ unsigned int c_bus1_event_get_type(CBus1Event *event) {
        return event->type;
}

/**
 * c_bus1_event_unwrap_kernel() - XXX
 */
_c_public_ void c_bus1_event_unwrap_kernel(CBus1Event *event,
                                           uint64_t *destinationp,
                                           struct bus1_message **messagep) {
        c_assert(event->type == C_BUS1_EVENT_TYPE_KERNEL);
        *destinationp = event->destination.id;
        *messagep = &event->message.kernel;
}

/**
 * c_bus1_event_unwrap_message() - XXX
 */
_c_public_ void c_bus1_event_unwrap_message(CBus1Event *event,
                                            CBus1Object **objectp,
                                            CBus1Message **messagep) {
        c_assert(event->type == C_BUS1_EVENT_TYPE_MESSAGE);
        *objectp = event->destination.object;
        *messagep = &event->message.user;
}

/**
 * c_bus1_event_unwrap_release() - XXX
 */
_c_public_ void c_bus1_event_unwrap_release(CBus1Event *event,
                                            CBus1Object **objectp) {
        c_assert(event->type == C_BUS1_EVENT_TYPE_RELEASE);
        *objectp = event->destination.object;
}

/**
 * c_bus1_event_unwrap_destruction() - XXX
 */
_c_public_ void c_bus1_event_unwrap_destruction(CBus1Event *event,
                                                CBus1Handle **handlep) {
        c_assert(event->type == C_BUS1_EVENT_TYPE_DESTRUCTION);
        *handlep = event->destination.handle;
}

static int c_bus1_handle_compare(CRBTree *tree, void *k, CRBNode *rb) {
        CBus1Handle *handle = c_container_of(rb, CBus1Handle, rb_b1);
        uint64_t *id = k;

        if (*id < handle->id)
                return -1;
        else if (*id > handle->id)
                return 1;
        else
                return 0;
}

static void c_bus1_handle_bind(CBus1Handle *handle, CBus1 *b1, uint64_t id) {
        CRBNode **slot, *parent;

        c_assert(id != BUS1_INVALID);
        c_assert(handle->id == BUS1_INVALID);

        slot = c_rbtree_find_slot(&handle->b1->map_handles,
                                  c_bus1_handle_compare,
                                  &id,
                                  &parent);
        c_assert(slot);

        c_rbtree_add(&handle->b1->map_handles, parent, slot, &handle->rb_b1);
        handle->id = id;
        handle->b1 = b1; /* not pinned; cleared on unlink or ctx free */
}

static void c_bus1_handle_unbind(CBus1Handle *handle, bool release) {
        int r;

        if (!handle->b1)
                return;

        if (release) {
                r = c_bus1_sys_release(handle->b1->fd,
                                       0,
                                       1,
                                       &handle->id);
                c_assert(!r);
        }

        c_rbnode_unlink(&handle->rb_b1);
        handle->b1 = NULL;
}

static int c_bus1_handle_new(CBus1Handle **handlep) {
        _c_cleanup_(c_bus1_handle_unrefp) CBus1Handle *handle = NULL;

        handle = calloc(1, sizeof(*handle));
        if (!handle)
                return -ENOMEM;

        *handle = (CBus1Handle)C_BUS1_HANDLE_NULL(*handle);

        *handlep = handle;
        handle = NULL;
        return 0;
}

/**
 * c_bus1_handle_ref() - XXX
 */
_c_public_ CBus1Handle *c_bus1_handle_ref(CBus1Handle *handle) {
        if (handle)
                ++handle->n_refs;
        return handle;
}

/**
 * c_bus1_handle_unref() - XXX
 */
_c_public_ CBus1Handle *c_bus1_handle_unref(CBus1Handle *handle) {
        if (!handle || --handle->n_refs)
                return NULL;

        c_bus1_handle_unbind(handle, true);
        free(handle);

        return NULL;
}

/**
 * c_bus1_handle_wrap() - XXX
 */
_c_public_ int c_bus1_handle_wrap(CBus1Handle **handlep, CBus1 *b1, uint64_t id) {
        _c_cleanup_(c_bus1_handle_unrefp) CBus1Handle *handle = NULL;
        int r;

        r = c_bus1_handle_new(&handle);
        if (r)
                return r;

        c_bus1_handle_bind(handle, b1, id);

        *handlep = handle;
        handle = NULL;
        return 0;
}

/**
 * c_bus1_handle_unwrap() - XXX
 */
_c_public_ uint64_t c_bus1_handle_unwrap(CBus1Handle *handle) {
        uint64_t id;

        id = handle->id;
        c_bus1_handle_unbind(handle, false);
        c_bus1_handle_unref(handle);

        return id;
}

/**
 * c_bus1_handle_get_id() - XXX
 */
_c_public_ uint64_t c_bus1_handle_get_id(CBus1Handle *handle) {
        return handle->id;
}

static int c_bus1_object_compare(CRBTree *tree, void *k, CRBNode *rb) {
        CBus1Object *object = c_container_of(rb, CBus1Object, rb_b1);
        uint64_t *id = k;

        if (*id < object->id)
                return -1;
        else if (*id > object->id)
                return 1;
        else
                return 0;
}

static int c_bus1_object_new(CBus1Object **objectp) {
        _c_cleanup_(c_bus1_object_unrefp) CBus1Object *object = NULL;

        object = calloc(1, sizeof(*object));
        if (!object)
                return -ENOMEM;

        *object = (CBus1Object)C_BUS1_OBJECT_NULL(*object);

        *objectp = object;
        object = NULL;
        return 0;
}

CBus1Object *c_bus1_object_ref(CBus1Object *object) {
        if (object)
                ++object->n_refs;
        return object;
}

CBus1Object *c_bus1_object_unref(CBus1Object *object) {
        if (!object || --object->n_refs)
                return NULL;

        c_assert(!c_rbnode_is_linked(&object->rb_b1));
        c_assert(!object->b1);

        free(object);

        return NULL;
}

static void c_bus1_object_bind(CBus1Object *object, CBus1 *b1, uint64_t id) {
        CRBNode **slot, *parent;

        c_assert(id != BUS1_INVALID);
        c_assert(object->id == BUS1_INVALID);

        slot = c_rbtree_find_slot(&b1->map_objects,
                                  c_bus1_object_compare,
                                  &id,
                                  &parent);
        c_assert(slot);

        c_rbtree_add(&b1->map_objects, parent, slot, &object->rb_b1);
        object->id = id;
        object->b1 = b1; /* not pinned; cleared on unlink or ctx free */
        c_bus1_object_ref(object);
}

static void c_bus1_object_unbind(CBus1Object *object, bool release) {
        int r;

        if (!object->b1)
                return;

        if (release && !object->destroyed) {
                r = c_bus1_sys_destroy(object->b1->fd,
                                       0,
                                       1,
                                       &object->id);
                c_assert(!r);
        }

        c_rbnode_unlink(&object->rb_b1);
        object->b1 = NULL;
        c_bus1_object_unref(object);
}

/**
 * c_bus1_object_release() - XXX
 */
_c_public_ CBus1Object *c_bus1_object_release(CBus1Object *object) {
        if (object) {
                c_assert(!object->released);
                object->released = true;
                c_bus1_object_destroy(object);
                c_bus1_object_unref(object);
        }
        return NULL;
}

/**
 * c_bus1_object_destroy() - XXX
 */
_c_public_ void c_bus1_object_destroy(CBus1Object *object) {
        int r;

        c_assert(object->id != BUS1_INVALID);

        if (!object->destroyed) {
                object->destroyed = true;
                if (object->b1) {
                        r = c_bus1_sys_destroy(object->b1->fd,
                                               0,
                                               1,
                                               &object->id);
                        c_assert(!r);
                }
        }
}

/**
 * c_bus1_object_wrap() - XXX
 */
_c_public_ int c_bus1_object_wrap(CBus1Object **objectp, CBus1 *b1, uint64_t id) {
        _c_cleanup_(c_bus1_object_unrefp) CBus1Object *object = NULL;
        int r;

        r = c_bus1_object_new(&object);
        if (r)
                return r;

        c_bus1_object_bind(object, b1, id);

        *objectp = object;
        object = NULL;
        return 0;
}

/**
 * c_bus1_object_unwrap() - XXX
 */
_c_public_ uint64_t c_bus1_object_unwrap(CBus1Object *object) {
        uint64_t id;

        id = object->id;
        c_bus1_object_unbind(object, false);
        c_bus1_object_unref(object);

        return id;
}

/**
 * c_bus1_object_get_userdata() - XXX
 */
_c_public_ void *c_bus1_object_get_userdata(CBus1Object *object) {
        return object->userdata;
}

/**
 * c_bus1_object_set_userdata() - XXX
 */
_c_public_ void c_bus1_object_set_userdata(CBus1Object *object, void *userdata) {
        object->userdata = userdata;
}

/**
 * c_bus1_object_get_id() - XXX
 */
_c_public_ uint64_t c_bus1_object_get_id(CBus1Object *object) {
        return object->id;
}

static void c_bus1_init(CBus1 *b1) {
        *b1 = (CBus1)C_BUS1_NULL(*b1);
}

static int c_bus1_deinit(CBus1 *b1, bool release) {
        CBus1Object *object, *t_object;
        CBus1Handle *handle, *t_handle;
        CBus1Event *event, *t_event;
        int fd;

        c_list_for_each_entry_safe(event, t_event, &b1->live_events, link_b1)
                c_bus1_event_unbind(event, release);

        c_list_for_each_entry_safe(event, t_event, &b1->incoming_events, link_b1) {
                c_bus1_event_unbind(event, release);
                c_bus1_event_unref(event);
        }

        c_list_for_each_entry_safe(event, t_event, &b1->unused_events, link_b1) {
                c_bus1_event_unbind(event, release);
                c_bus1_event_unref(event);
        }

        c_rbtree_for_each_entry_safe_postorder_unlink(handle,
                                                      t_handle,
                                                      &b1->map_handles,
                                                      rb_b1)
                c_bus1_handle_unbind(handle, release);

        c_rbtree_for_each_entry_safe_postorder_unlink(object,
                                                      t_object,
                                                      &b1->map_objects,
                                                      rb_b1)
                c_bus1_object_unbind(object, release);

        c_assert(c_rbtree_is_empty(&b1->map_objects));
        c_assert(c_rbtree_is_empty(&b1->map_handles));
        c_assert(c_list_is_empty(&b1->unused_events));
        c_assert(c_list_is_empty(&b1->incoming_events));
        c_assert(c_list_is_empty(&b1->live_events));
        c_assert(!b1->n_unused_events);

        fd = b1->fd;
        b1->fd = -1;
        return fd;
}

/**
 * c_bus1_new() - XXX
 */
_c_public_ int c_bus1_new(CBus1 **b1p) {
        _c_cleanup_(c_bus1_freep) CBus1 *b1 = NULL;
        int r;

        b1 = calloc(1, sizeof(*b1));
        if (!b1)
                return -ENOMEM;

        c_bus1_init(b1);

        r = c_bus1_sys_open(&b1->fd);
        if (r)
                return (r < 0) ? r : -ENOTRECOVERABLE;

        *b1p = b1;
        b1 = NULL;
        return 0;
}

/**
 * c_bus1_free() - XXX
 */
_c_public_ CBus1 *c_bus1_free(CBus1 *b1) {
        if (b1) {
                c_bus1_sys_close(c_bus1_deinit(b1, false));
                free(b1);
        }
        return NULL;
}

/**
 * c_bus1_wrap() - XXX
 */
_c_public_ int c_bus1_wrap(CBus1 **b1p, int fd) {
        _c_cleanup_(c_bus1_freep) CBus1 *b1 = NULL;

        b1 = calloc(1, sizeof(*b1));
        if (!b1)
                return -ENOMEM;

        c_bus1_init(b1);
        b1->fd = fd;

        *b1p = b1;
        b1 = NULL;
        return 0;
}

/**
 * c_bus1_unwrap() - XXX
 */
_c_public_ int c_bus1_unwrap(CBus1 *b1) {
        int fd;

        fd = c_bus1_deinit(b1, true);
        free(b1);
        return fd;
}

CBus1Object *c_bus1_find_raw_object(CBus1 *b1, uint64_t id) {
        return c_rbtree_find_entry(&b1->map_objects,
                                   c_bus1_object_compare,
                                   &id,
                                   CBus1Object,
                                   rb_b1);
}

CBus1Handle *c_bus1_find_raw_handle(CBus1 *b1, uint64_t id) {
        return c_rbtree_find_entry(&b1->map_handles,
                                   c_bus1_handle_compare,
                                   &id,
                                   CBus1Handle,
                                   rb_b1);
}

/**
 * c_bus1_find_object() - XXX
 */
_c_public_ CBus1Object *c_bus1_find_object(CBus1 *b1, uint64_t id) {
        CBus1Object *object;

        object = c_bus1_find_raw_object(b1, id);
        if (object && !object->released)
                return object;

        return NULL;
}

/**
 * c_bus1_find_handle() - XXX
 */
_c_public_ CBus1Handle *c_bus1_find_handle(CBus1 *b1, uint64_t id) {
        return c_bus1_find_raw_handle(b1, id);
}

/**
 * c_bus1_get_fd() - XXX
 */
_c_public_ int c_bus1_get_fd(CBus1 *b1) {
        return b1->fd;
}

/**
 * c_bus1_dispatch() - XXX
 */
_c_public_ int c_bus1_dispatch(CBus1 *b1) {
        const size_t max = 16;
        CBus1Event *event;
        size_t i;
        int r;

        /*
         * XXX: We fetch 16 messages at a time right now. This is more or less
         *      chosen at random and serves as a placeholder. We really should
         *      improve on this and benchmark whether this really speeds things
         *      up. We definitely want to keep the infrastructure for
         *      batch-operation in place, since there are ideas to expand the
         *      kernel API to return multiple messages at once. Once that lands
         *      batch operation will definitely speed things up.
         *      For now, we keep the static prefetcher in place to at least
         *      make sure our infrastructure works fine with batched and cached
         *      message dispatching.
         */

        for (i = 0; i < max; ++i) {
                event = c_list_first_entry(&b1->unused_events,
                                           CBus1Event,
                                           link_b1);
                if (!event) {
                        r = c_bus1_event_new(&event);
                        if (r)
                                return r;

                        c_bus1_event_bind(event, b1);
                }

                event->message.kernel = (struct bus1_message){
                        .flags = 0,
                        .type = BUS1_INVALID,
                        .n_transfers = 0,
                        .ptr_transfers = (unsigned long)NULL,
                        .n_data = 0,
                        .n_data_vecs = 0,
                        .ptr_data_vecs = (unsigned long)NULL,
                };

                r = c_bus1_sys_recv(b1->fd,
                                    0,
                                    &event->destination.id,
                                    &event->message.kernel);
                if (r) {
                        if (r == -EAGAIN)
                                return 0;

                        return (r < 0) ? r : -ENOTRECOVERABLE;
                }

                c_bus1_event_claim(event);
                event->type = C_BUS1_EVENT_TYPE_KERNEL;
        }

        return C_BUS1_E_PREEMPTED;
}

static int c_bus1_fetch(CBus1 *b1, CBus1Event **eventp) {
        CBus1Event *event = *eventp;
        CBus1Object *object;
        CBus1Handle *handle;
        size_t i;
        int r;

        /*
         * Most of the time messages on the incoming queue can just be fetched
         * and forwarded straight to the caller. Especially in case of events
         * on unknown objects/handles, we always forward it unmodified. We
         * allow the caller to manage their own objects/handles independent of
         * this context.
         * However, events on managed objects are properly parsed and handled
         * first. This includes discarding events on released objects,
         * converting IDs into references to actual objects, and more.
         */

        c_assert(event->type == C_BUS1_EVENT_TYPE_KERNEL);

        switch (event->message.kernel.type) {
        case BUS1_MESSAGE_TYPE_CUSTOM: {
                CBus1Transfer *transfers;
                CBus1Message *message;

                object = c_bus1_find_raw_object(b1, event->destination.id);
                if (!object)
                        break;
                if (object->released) {
                        *eventp = NULL;
                        break;
                }

                message = &event->message.user;
                transfers = (void *)(unsigned long)message->ptr_transfers.u64;
                for (i = 0; i < message->n_transfers; ++i) {
                        handle = c_bus1_find_raw_handle(b1, transfers[i].id.u64);
                        if (handle) {
                                /*
                                 * We already have a reference, so lets drop
                                 * the additional kernel reference that we got
                                 * and rather take a local reference.
                                 */
                                r = c_bus1_sys_release(b1->fd,
                                                       0,
                                                       1,
                                                       &transfers[i].id.u64);
                                c_assert(!r);
                                c_bus1_handle_ref(handle);
                        } else {
                                /*
                                 * We do not have a reference, yet. Wrap the ID
                                 * in a new handle so we can track it.
                                 */
                                r = c_bus1_handle_wrap(&handle,
                                                       b1,
                                                       transfers[i].id.u64);
                        }

                        if (r) {
                                while (i--) {
                                        c_bus1_handle_unref(transfers[i].id.handle);
                                        transfers[i].id.u64 = BUS1_INVALID;
                                }

                                return r;
                        }

                        transfers[i].id.handle = handle;
                }

                event->type = C_BUS1_EVENT_TYPE_MESSAGE;
                event->destination.object = c_bus1_object_ref(object);
                break;
        }

        case BUS1_MESSAGE_TYPE_RELEASE: {
                bool released;

                object = c_bus1_find_raw_object(b1, event->destination.id);
                if (!object)
                        break;

                /*
                 * We got the kernel confirmation that the object is now fully
                 * released. This means, we drop it from the rb-tree and shut
                 * down all operations on it. It is only left for the owner to
                 * clean up. If the owner already released it themself, this
                 * will actually drop the last reference. Otherwise, there must
                 * be at least one reference left (the one held by the owner).
                 */
                released = object->released;
                c_bus1_object_unbind(object, false);

                if (released) {
                        *eventp = NULL;
                        break;
                }

                event->type = C_BUS1_EVENT_TYPE_RELEASE;
                event->destination.object = c_bus1_object_ref(object);
                break;
        }

        case BUS1_MESSAGE_TYPE_DESTRUCTION: {
                handle = c_bus1_find_raw_handle(b1, event->destination.id);
                if (!handle)
                        break;

                c_bus1_handle_unbind(handle, false);

                event->type = C_BUS1_EVENT_TYPE_DESTRUCTION;
                event->destination.handle = c_bus1_handle_ref(handle);
                break;
        }

        default:
                /* forward everything else unmodified */
                break;
        }

        c_bus1_event_publish(event);
        if (!*eventp)
                c_bus1_event_unref(event);

        return 0;
}

/**
 * c_bus1_pop_event() - XXX
 */
_c_public_ int c_bus1_pop_event(CBus1 *b1, CBus1Event **eventp) {
        CBus1Event *event;
        int r;

        while ((event = c_list_first_entry(&b1->incoming_events,
                                           CBus1Event,
                                           link_b1))) {
                r = c_bus1_fetch(b1, &event);
                if (r)
                        return r;
                if (event)
                        break;
        }

        *eventp = event;
        return 0;
}

/**
 * c_bus1_pair() - XXX
 */
_c_public_ int c_bus1_pair(CBus1 *b1,
                           CBus1Object **objectp,
                           CBus1Handle **handlep) {
        _c_cleanup_(c_bus1_object_unrefp) CBus1Object *object = NULL;
        _c_cleanup_(c_bus1_handle_unrefp) CBus1Handle *handle = NULL;
        uint64_t object_id, handle_id;
        int r;

        r = c_bus1_object_new(&object);
        if (r)
                return r;

        r = c_bus1_handle_new(&handle);
        if (r)
                return r;

        r = c_bus1_sys_pair(b1->fd, -1, 0, &object_id, &handle_id);
        if (r)
                return (r < 0) ? r : -ENOTRECOVERABLE;

        c_bus1_object_bind(object, b1, object_id);
        c_bus1_handle_bind(handle, b1, handle_id);

        *objectp = object;
        *handlep = handle;
        object = NULL;
        handle = NULL;
        return 0;
}
