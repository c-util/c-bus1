c-bus1
======

Bus1 Capability-based IPC Bindings for ISO-C11

The c-bus1 project provides access to the bus1 APIs from standard C code. The
bus1 APIs implement object-oriented Inter-Process Communication in a scalable
way for services and operating system tasks to share signals, data, and
resources, while at the same time providing maximum isolation of the different
communication entities.

### Project

 * **Website**: <https://c-util.github.io/c-bus1>
 * **Bug Tracker**: <https://github.com/c-util/c-bus1/issues>

### Requirements

The requirements for this project are:

 * `libc` (e.g., `glibc >= 2.16`)

At build-time, the following software is required:

 * `meson >= 0.41`
 * `pkg-config >= 0.29`

### Build

The meson build-system is used for this project. Contact upstream
documentation for detailed help. In most situations the following
commands are sufficient to build and install from source:

```sh
mkdir build
cd build
meson setup ..
ninja
meson test
ninja install
```

No custom configuration options are available.

### Repository:

 - **web**:   <https://github.com/c-util/c-bus1>
 - **https**: `https://github.com/c-util/c-bus1.git`
 - **ssh**:   `git@github.com:c-util/c-bus1.git`

### License:

 - **Apache-2.0** OR **LGPL-2.1-or-later**
 - See AUTHORS file for details.
