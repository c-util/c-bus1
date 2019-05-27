/*
 * Bus1 Kernel-API Access
 *
 * This file contains the implementation of the bus1 kernel-API accessors. All
 * kernel functionality of the bus1 module is wrapped in a proper C API to
 * provide type-safe access and to hide the specifics of the user/kernel
 * boundary.
 *
 * Currently, the character-device ioctl-based bus1 API is used, but this can
 * be adapted to a syscall-based API without API breaks.
 */

#include <assert.h>
#include <c-stdaux.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "bus1.h"
#include "c-bus1.h"
#include "c-bus1-private.h"

/**
 * c_bus1_sys_open() - open new bus1 descriptor
 * @fdp:                output argument for new descriptor
 *
 * This opens a new bus1 descriptor and returns it to the caller in @fdp.
 *
 * Return: 0 on success, negative kernel error code on failure.
 */
_c_public_ int c_bus1_sys_open(int *fdp) {
        _c_cleanup_(c_closep) int fd = -1;

        fd = open("/dev/bus1", O_RDWR | O_CLOEXEC | O_NONBLOCK | O_NOCTTY);
        if (fd < 0)
                return -errno;

        *fdp = fd;
        fd = -1;
        return 0;
}

/**
 * c_bus1_sys_close() - close a bus1 descriptor
 * @fd:                 descriptor to operate on, or -1
 *
 * This closes a bus1 descriptor previously acquired via c_bus1_sys_open(). The
 * caller must cease any operations on @fd.
 *
 * If @fd is -1, this is a no-op.
 *
 * Return: -1 is returned.
 */
_c_public_ int c_bus1_sys_close(int fd) {
        return c_close(fd);
}

/**
 * c_bus1_sys_pair() - pair an object and a handle
 * @fd1:                descriptor to create object on
 * @fd2:                descriptor to create handle on, or -1
 * @flags:              operation modifiers
 * @objectp:            output argument for object ID
 * @handlep:            output argument for handle ID
 *
 * This pairs a new handle with a new object. It creates the object on @fd1 and
 * returns the new object ID in @objectp. A new initial handle for the new
 * object is created on @fd2 (unless @fd2 is -1, in which case it is created on
 * @fd1 as well) and its ID returned in @handlep.
 *
 * Return: 0 on success, negative kernel error code on failure.
 */
_c_public_ int c_bus1_sys_pair(int fd1,
                               int fd2,
                               uint64_t flags,
                               uint64_t *objectp,
                               uint64_t *handlep) {
        struct bus1_cmd_pair cmd_pair = {
                .flags = flags,
                .fd2 = fd2,
                .object_id = BUS1_INVALID,
                .handle_id = BUS1_INVALID,
        };
        int r;

        r = ioctl(fd1, BUS1_CMD_PAIR, &cmd_pair);
        if (r < 0)
                return -errno;

        *objectp = cmd_pair.object_id;
        *handlep = cmd_pair.handle_id;
        return r;
}

/**
 * c_bus1_sys_send() - send a message to the specified targets
 * @fd:                 descriptor to operate on
 * @flags:              operation modifiers
 * @n_destinations:     number of destinations
 * @destinations:       array of destination handle IDs
 * @errors:             output array for errors on each destination, or NULL
 * @message:            message to send
 *
 * This sends a message given as @message to the destinations specified in
 * @destinations (array of @n_destinations entries). A destination is provided
 * as a handle ID, and the message will be transferred to the owner of the
 * object the handle is paired with.
 *
 * If @errors is non-NULL, it must point to a local array of @n_destinations
 * entries. It is used to store individual error-codes of each destination.
 * Note that by default it is only filled in if the send operation fails,
 * otherwise it is left untouched.
 *
 * Return: 0 on success, negative kernel error code on failure.
 */
_c_public_ int c_bus1_sys_send(int fd,
                               uint64_t flags,
                               uint64_t n_destinations,
                               const uint64_t *destinations,
                               int *errors,
                               const struct bus1_message *message) {
        struct bus1_cmd_send cmd_send = {
                .flags = flags,
                .n_destinations = n_destinations,
                .ptr_destinations = (unsigned long)destinations,
                .ptr_errors = (unsigned long)errors,
                .ptr_message = (unsigned long)message,
        };

        return ioctl(fd, BUS1_CMD_SEND, &cmd_send);
}

/**
 * c_bus1_sys_recv() - receive the next pending message
 * @fd:                 descriptor to operate on
 * @flags:              operation modifiers
 * @destinationp:       output argument for the destination ID
 * @message:            message buffer location
 *
 * This receives the next incoming message on the bus1 descriptor @fd. If no
 * more messages are pending, this will return -EAGAIN.
 *
 * If there is a pending message, the local ID of the object or handle the
 * message targetted is returned in @destinationp (note that it depends on the
 * message-type whether the target is an object or a handle). The message
 * metadata is written into @message. The message content is written to the
 * buffers specified in @message.
 *
 * Return: 0 on success, negative kernel error code on failure.
 */
_c_public_ int c_bus1_sys_recv(int fd,
                               uint64_t flags,
                               uint64_t *destinationp,
                               struct bus1_message *message) {
        struct bus1_cmd_recv cmd_recv = {
                .flags = flags,
                .destination = BUS1_INVALID,
                .ptr_message = (unsigned long)message,
        };
        int r;

        r = ioctl(fd, BUS1_CMD_RECV, &cmd_recv);
        if (r < 0)
                return -errno;

        *destinationp = cmd_recv.destination;
        return r;
}

/**
 * c_bus1_sys_destroy() - destroy objects
 * @fd:                 descriptor to operate on
 * @flags:              operation modifiers
 * @n_objects:          number of objects
 * @objects:            array of object IDs
 *
 * This triggers an atomic destruction of all the objects listed in @objects.
 * An object can only be destroyed once. A destruction notification is send to
 * all peers that hold a handle. Furthermore, a release notification is sent
 * back to this peer as ordered confirmation of the destruction.
 *
 * Return: 0 on success, negative kernel error code on failure.
 */
_c_public_ int c_bus1_sys_destroy(int fd,
                                  uint64_t flags,
                                  uint64_t n_objects,
                                  const uint64_t *objects) {
        struct bus1_cmd_destroy cmd_destroy = {
                .flags = flags,
                .n_objects = n_objects,
                .ptr_objects = (unsigned long)objects,
        };

        return ioctl(fd, BUS1_CMD_DESTROY, &cmd_destroy);
}

/**
 * c_bus1_sys_acquire() - acquire handle references
 * @fd:                 descriptor to operate on
 * @flags:              operation modifiers
 * @n_handles:          number of handles
 * @handles:            array of handle IDs
 *
 * This atomically acquires a handle-reference to each handle listed in the
 * handle-array @handles. An individual handle ID can be listed multiple times
 * in the array to acquire multiple references to the same handle.
 *
 * Return: 0 on success, negative kernel error code on failure.
 */
_c_public_ int c_bus1_sys_acquire(int fd,
                                  uint64_t flags,
                                  uint64_t n_handles,
                                  const uint64_t *handles) {
        struct bus1_cmd_acquire cmd_acquire = {
                .flags = flags,
                .n_handles = n_handles,
                .ptr_handles = (unsigned long)handles,
        };

        return ioctl(fd, BUS1_CMD_ACQUIRE, &cmd_acquire);
}

/**
 * c_bus1_sys_release() - release handle references
 * @fd:                 descriptor to operate on
 * @flags:              operation modifiers
 * @n_handles:          number of handles
 * @handles:            array of handle IDs
 *
 * This atomically releases a single reference to each handle listed in the
 * handle array. An individual handle ID can be listed multiple times to
 * release multiple references to that handle.
 *
 * Return: 0 on success, negative kernel error code on failure.
 */
_c_public_ int c_bus1_sys_release(int fd,
                                  uint64_t flags,
                                  uint64_t n_handles,
                                  const uint64_t *handles) {
        struct bus1_cmd_release cmd_release = {
                .flags = flags,
                .n_handles = n_handles,
                .ptr_handles = (unsigned long)handles,
        };

        return ioctl(fd, BUS1_CMD_RELEASE, &cmd_release);
}
