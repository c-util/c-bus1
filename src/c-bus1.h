#pragma once

/**
 * Bus1 Capability-based IPC Bindings for ISO-C11
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <sys/uio.h>

struct bus1_message;
struct bus1_transfer;

typedef struct CBus1 CBus1;
typedef struct CBus1Event CBus1Event;
typedef struct CBus1Handle CBus1Handle;
typedef struct CBus1Message CBus1Message;
typedef struct CBus1Object CBus1Object;
typedef struct CBus1Transfer CBus1Transfer;

enum {
        _C_BUS1_E_SUCCESS,
        C_BUS1_E_PREEMPTED,
        _C_BUS1_E_N,
};

enum {
        C_BUS1_EVENT_TYPE_KERNEL,
        C_BUS1_EVENT_TYPE_MESSAGE,
        C_BUS1_EVENT_TYPE_RELEASE,
        C_BUS1_EVENT_TYPE_DESTRUCTION,
        _C_BUS1_EVENT_TYPE_N,
};

struct CBus1Transfer {
        uint64_t flags;
        union {
                uint64_t u64;
                CBus1Handle *handle;
        } id;
};

struct CBus1Message {
        uint64_t flags;
        uint64_t type;
        uint64_t n_transfers;
        union {
                uint64_t u64;
                CBus1Transfer *ptr;
        } ptr_transfers;
        uint64_t n_data;
        uint64_t n_data_vecs;
        union {
                uint64_t u64;
                struct iovec *ptr;
        } ptr_data_vecs;
};

/* events */

CBus1Event *c_bus1_event_ref(CBus1Event *event);
CBus1Event *c_bus1_event_unref(CBus1Event *event);

unsigned int c_bus1_event_get_type(CBus1Event *event);
void c_bus1_event_unwrap_kernel(CBus1Event *event,
                               uint64_t *destinationp,
                               struct bus1_message **messagep);
void c_bus1_event_unwrap_message(CBus1Event *event,
                                CBus1Object **objectp,
                                CBus1Message **messagep);
void c_bus1_event_unwrap_release(CBus1Event *event,
                                 CBus1Object **objectp);
void c_bus1_event_unwrap_destruction(CBus1Event *event,
                                     CBus1Handle **handlep);

/* handles */

CBus1Handle *c_bus1_handle_ref(CBus1Handle *handle);
CBus1Handle *c_bus1_handle_unref(CBus1Handle *handle);

int c_bus1_handle_wrap(CBus1Handle **handlep, CBus1 *b1, uint64_t id);
uint64_t c_bus1_handle_unwrap(CBus1Handle *handle);

uint64_t c_bus1_handle_get_id(CBus1Handle *handle);

/* objects */

CBus1Object *c_bus1_object_release(CBus1Object *object);
void c_bus1_object_destroy(CBus1Object *object);

int c_bus1_object_wrap(CBus1Object **objectp, CBus1 *b1, uint64_t id);
uint64_t c_bus1_object_unwrap(CBus1Object *object);

void *c_bus1_object_get_userdata(CBus1Object *object);
void c_bus1_object_set_userdata(CBus1Object *object, void *userdata);

uint64_t c_bus1_object_get_id(CBus1Object *object);

/* contexts */

int c_bus1_new(CBus1 **b1p);
CBus1 *c_bus1_free(CBus1 *b1);

int c_bus1_wrap(CBus1 **b1p, int fd);
int c_bus1_unwrap(CBus1 *b1);

CBus1Object *c_bus1_find_object(CBus1 *, uint64_t id);
CBus1Handle *c_bus1_find_handle(CBus1 *, uint64_t id);

int c_bus1_get_fd(CBus1 *b1);
int c_bus1_dispatch(CBus1 *b1);
int c_bus1_pop_event(CBus1 *b1, CBus1Event **eventp);

int c_bus1_pair(CBus1 *b1,
                CBus1Object **objectp,
                CBus1Handle **handlep);
int c_bus1_send(CBus1 *b1,
                size_t n_destinations,
                uint64_t *destination_ids,
                int *destination_errors,
                CBus1Message *message);

/* syscalls */

int c_bus1_sys_open(int *fdp);
int c_bus1_sys_close(int fd);

int c_bus1_sys_pair(int fd1,
                    int fd2,
                    uint64_t flags,
                    uint64_t *objectp,
                    uint64_t *handlep);
int c_bus1_sys_send(int fd,
                    uint64_t flags,
                    uint64_t n_destinations,
                    const uint64_t *destinations,
                    int *errors,
                    const struct bus1_message *message);
int c_bus1_sys_recv(int fd,
                    uint64_t flags,
                    uint64_t *destinationp,
                    struct bus1_message *message);
int c_bus1_sys_destroy(int fd,
                       uint64_t flags,
                       uint64_t n_objects,
                       const uint64_t *objects);
int c_bus1_sys_acquire(int fd,
                       uint64_t flags,
                       uint64_t n_handles,
                       const uint64_t *handles);
int c_bus1_sys_release(int fd,
                       uint64_t flags,
                       uint64_t n_handles,
                       const uint64_t *handles);

/* inline helpers */

static inline void c_bus1_event_unrefp(CBus1Event **event) {
        if (*event)
                c_bus1_event_unref(*event);
}

static inline void c_bus1_handle_unrefp(CBus1Handle **handle) {
        if (*handle)
                c_bus1_handle_unref(*handle);
}

static inline void c_bus1_object_releasep(CBus1Object **object) {
        if (*object)
                c_bus1_object_release(*object);
}

static inline void c_bus1_freep(CBus1 **b1) {
        if (*b1)
                c_bus1_free(*b1);
}

static inline void c_bus1_sys_closep(int *fd) {
        if (*fd >= 0)
                c_bus1_sys_close(*fd);
}

#ifdef __cplusplus
}
#endif
