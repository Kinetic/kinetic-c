[![Build Status](http://travis-ci.org/Kinetic/kinetic-c.png?branch=master)](http://travis-ci.org/Kinetic/kinetic-c)
Kinetic C Client Library
========================
The [Github kinetic-c Git repository](https://github.com/Kinetic/kinetic-c) contains code for producing Kinetic C clients for interacting with Kinetic storage object-based storage. The library uses the cross-platform Seagate Kinetic protocol for standardizing interaces between the Java simulator and Kinetic Device storage clusters.

Reference code is included as part of the [kinetic-c client library test utility (`kinetic-c-util`)](src/utility), which builds and links against the installed `kinetic-c-client` static library. [Additional examples](src/examples) are included for the various types of I/O operations (e.g. blocking/non-blocking, single/multi-threaded). See below for more details.

The [project Makefile](Makefile) can be used as a reference for developing a Makefile-based project for building for a custom Kinetic Storage C client driver and/or a high-level C library.

The C library currently does not support Windows at this time because of existing library requirements. If you need Windows please post an issue.

> The library has been tested in OSX and Linux, currently it does **not** have support for Windows (see [#23](https://github.com/Kinetic/kinetic-c/issues/23)).

Kinetic Protocol Support
------------------------
Built using:

* [Kinetic Protocol v3.0.5](https://github.com/Kinetic/kinetic-protocol/tree/3.0.5)
* [ProtoBuf-C v1.1.0](https://github.com/protobuf-c/protobuf-c)
* [Google ProtoBuf v2.6.0](https://developers.google.com/protocol-buffers/docs/downloads)

Prerequisites
-------------

* [Open SSL](https://www.openssl.org/) for security and encryption
    * Installation (if you don't already have OpenSSL installed)
        * Linux (using apt-get)
            * `> sudo apt-get install openssl`
        * Linux (using yum)
            * `> sudo yum install openssl`
        * OSX (using [Homebrew](http://brew.sh/))
            * `> brew install openssl`
* [json-c](https://github.com/json-c/json-c) for JSON-formatted ACL definition files
    * Installation
        * via package manager
            * Linux (using apt-get)
                * `> sudo apt-get install json-c`
            * Linux (using yum)
                * `> sudo yum install json-c`
            * OSX (using [Homebrew](http://brew.sh/))
                * `> brew install openssl`
        * via Git submodule (from bundled source)
            * `> make json`
            * `> sudo make install_json`

A release of OpenSSL that provides TLS 1.1 or newer is required.

If the OpenSSL installation is not found, the `OPENSSL_PATH` environment
variable may need to be set to its base path, e.g.
`export OPENSSL_PATH=/usr/local/openssl/1.0.1k/`.

Getting Started
---------------

**Clone the repo**

    > git clone --recursive https://github.com/Kinetic/kinetic-c.git
    > cd kinetic-c

**Update to the latest version (previously cloned)**

    > git pull
    > make config # ensures all git submodules are up to date

**Build and install static library**

    > make
    > sudo make install

**Clean and uninstall any old versions**

    > make clean
    > sudo make uninstall

**Build example utility and run tests against Kinetic Device simulator**

    > make start_sims # starts bundled kinetic-java simulators for testing
    > make all # this is what Travis-CI build does does for regression testing
    > make stop_sims # stops all locally running simulators

API Documentation
=================

[Kinetic-C API Documentation](http://seagate.github.io/kinetic-c/) (generated with Doxygen)
* [Kinetic-C API](http://seagate.github.io/kinetic-c/kinetic__client_8h.html)
* [Kinetic-C types](http://seagate.github.io/kinetic-c/kinetic__types_8h.html)
* [ByteArray API](http://seagate.github.io/kinetic-c/byte__array_8h.html)
    * The ByteArray and ByteBuffer types are used for exchanging variable length byte-arrays with kinetic-c
        * e.g. object keys, object value data, etc.

**NOTE: Configuration structures `KineticClientConfig` and `KineticSessionConfig` should be initialized per C99 struct initialization or memset to 0 prior to use in order to ensure forward/backward compatibility upon changes to these structure definitions!**

[Developer Documentation](DEVELOP.md)

Client Test Utility
===========================

Code examples are included for reference as part of a test utility. The source code for the utility is used to build both a static and dynamically linked verion of the kinetic-c-client library.

* `kinetic-c-util` builds/links against Kinetic C static library (.a)

Usage
----------

    $ cd bin
    $ ./kinetic-c-util --help
    Usage: ./bin/kinetic-c-util --<cmd> [options...]
    ./bin/kinetic-c-util --help
    ./bin/kinetic-c-util --noop [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>]
    ./bin/kinetic-c-util --put --key <key> --value <value> [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>]
    ./bin/kinetic-c-util --get --key <key> [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>]
    ./bin/kinetic-c-util --getnext --key <key> [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>]
    ./bin/kinetic-c-util --getprevious --key <key> [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>]
    ./bin/kinetic-c-util --delete --key <key> [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>]
    ./bin/kinetic-c-util --getlog --logtype <utilizations|temperatures|capacities|configuration|statistics|messages|limits> [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]
    ./bin/kinetic-c-util --getdevicespecificlog --devicelogname <name|ID> [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]
    ./bin/kinetic-c-util --setclusterversion --newclusterversion <newclusterversion> [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]
    ./bin/kinetic-c-util --seterasepin --pin <oldpin> --newpin <newerasepin> [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]
    ./bin/kinetic-c-util --instanterase --pin <erasepin> [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]
    ./bin/kinetic-c-util --secureerase --pin <erasepin> [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]
    ./bin/kinetic-c-util --setlockpin --pin <oldpin>> <--newpin <newpin> [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]
    ./bin/kinetic-c-util --lockdevice --pin <lockpin> [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]
    ./bin/kinetic-c-util --unlockdevice --pin <lockpin> [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]
    ./bin/kinetic-c-util --setacl --file <acl_json_file> [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]
    ./bin/kinetic-c-util --updatefirmware --file <file> --pin <pin> [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>]

Kinetic C Client I/O Examples
=============================

* [`blocking_put_get`](src/examples/blocking_put_get.c) - Blocking put w/get.
* [`blocking_put_delete`](src/examples/blocking_put_delete.c) - Blocking put w/delete.
* [`put_nonblocking`](src/examples/put_nonblocking.c) - Single thread, single connection, nonblocking put operation.
* [`get_nonblocking`](src/examples/get_nonblocking.c) - Single thread, single connection, nonblocking get operation.
* [`get_key_range`](src/examples/get_key_range.c) - Query a range of keys from a device.
* [`write_file_blocking`](src/examples/write_file_blocking.c) - Single thread, single connection, blocking operation.
* [`write_file_blocking_threads`](src/examples/write_file_blocking_threads.c) - Multiple threads, single connection, blocking operations.
* [`write_file_nonblocking`](src/examples/write_file_nonblocking.c) - Single thread, single connection, multiple non-blocking operations
* [`write_file_blocking_threads`](src/examples/write_file_blocking_threads.c) - Multiple threads, single connection, multiple non-blocking operations.
