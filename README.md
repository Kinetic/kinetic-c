[![Build Status](http://travis-ci.org/Seagate/kinetic-c.png?branch=master)](http://travis-ci.org/Seagate/kinetic-c)
Kinetic C Client Library
========================
The [Github kinetic-c Git repository](https://github.com/Seagate/kinetic-c) contains code for producing Kinetic C clients for interacting with Kinetic storage object-based storage. The library uses the cross-platform Seagate Kinetic protocol for standardizing interaces between the Java simulator and Kinetic Device storage clusters.

Reference code is included as part of the [kinetic-c client library test utility (`kinetic-c-util`)](src/utility), which builds and links against the installed `kinetic-c-client` static library. [Additional examples](src/examples) are included for the various types of I/O operations (e.g. blocking/non-blocking, single/multi-threaded). See below for more details.

The [project Makefile](Makefile) can be used as a reference for developing a Makefile-based project for building for a custom Kinetic Storage C client driver and/or a high-level C library.

Kinetic Protocol Support
------------------------
Built using [Kinetic Protocol v3.0.5](https://github.com/Seagate/kinetic-protocol/tree/3.0.5)

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
        * Linux (using apt-get)
            * `> sudo apt-get install json-c`
        * Linux (using yum)
            * `> sudo yum install json-c`
        * OSX (using [Homebrew](http://brew.sh/))
            * `> brew install openssl`
            
A release of OpenSSL that provides TLS 1.1 or newer is required.

If the OpenSSL installation is not found, the `OPENSSL_PATH` environment
variable may need to be set to its base path, e.g.
`export OPENSSL_PATH=/usr/local/openssl/1.0.1k/`.

Getting Started
---------------

**Clone the repo**

    > git clone --recursive https://github.com/seagate/kinetic-c.git
    > cd kinetic-c
    > bundle install # ensure you have all RubyGems at the proper versions

**Update to the latest version (previously cloned)**

    > git pull
    > make clean

**Build and install static library**

    > make
    > sudo make install

**Clean and uninstall old versions**

    > make clean
    > sudo make uninstall

**Build example utility and run tests against Kinetic Device simulator**

    > make all # this is what Travis-CI build does does for regression testing

API Documentation
=================

[Kinetic-C API Documentation](http://seagate.github.io/kinetic-c/) (generated with Doxygen)
* [Kinetic-C API](http://seagate.github.io/kinetic-c/kinetic__client_8h.html)
* [Kinetic-C types](http://seagate.github.io/kinetic-c/kinetic__types_8h.html)
* [ByteArray API](http://seagate.github.io/kinetic-c/byte__array_8h.html)
    * The ByteArray and ByteBuffer types are used for exchanging variable length byte-arrays with kinetic-c
        * e.g. object keys, object value data, etc.

**NOTE: Configuration structures `KineticClientConfig` and `KineticSessionConfig` should be initialized per C99 struct initialization or memset to 0 prior to use in order to ensure forward/backward compatibility upon changes to these structure definitions!**

Client Test Utility
===========================

Code examples are included for reference as part of a test utility. The source code for the utility is used to build both a static and dynamically linked verion of the kinetic-c-client library.

* `kinetic-c-util` builds/links against Kinetic C static library (.a)
* `kinetic-c-util.x.y.z` builds/links against Kinetic C dynamic library (.so)

Usage
----------

    $ cd bin
    $ ./kinetic-c-util --help
    Usage: ./kinetic-c-util --<cmd> [options...]
    ./kinetic-c-util --help
    ./kinetic-c-util --noop [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>]
    ./kinetic-c-util --put [--key <key>] [--value <value>] [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>]
    ./kinetic-c-util --get [--key <key>] [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>]
    ./kinetic-c-util --getnext [--key <key>] [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>]
    ./kinetic-c-util --getprevious [--key <key>] [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>]
    ./kinetic-c-util --delete [--key <key>] [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>]
    ./kinetic-c-util --setclusterversion <--newclusterversion <newclusterversion>> [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]
    ./kinetic-c-util --seterasepin <--pin <oldpin>> <--newpin <newerasepin>> [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]
    ./kinetic-c-util --instanterase <--pin <erasepin>> [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]
    ./kinetic-c-util --secureerase <--pin <erasepin>> [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]
    ./kinetic-c-util --setlockpin <--pin <oldpin>> <--newpin <newpin>> [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]
    ./kinetic-c-util --lockdevice <--pin <lockpin>> [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]
    ./kinetic-c-util --unlockdevice <--pin <lockpin>> [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]
    ./kinetic-c-util --setacl <--file <acl_json_file>> [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]
    ./kinetic-c-util --getlog [--type <utilization|temperature|capacity|configuration|message|statistic|limits> [--host <ip|hostname>] [--tlsport <tlsport>] [--clusterversion <clusterversion>]
    ./kinetic-c-util --updatefirmware <--file <file>> [--host <ip|hostname>] [--port <port>] [--clusterversion <clusterversion>] [--pin <pin>]

Kinetic C Client I/O Examples
=============================

* [`put_nonblocking`](src/examples/put_nonblocking.c) - Single thread, single connection, nonblocking put operation.
* [`get_nonblocking`](src/examples/get_nonblocking.c) - Single thread, single connection, nonblocking get operation.
* [`write_file_blocking`](src/examples/write_file_blocking.c) - Single thread, single connection, blocking operation.
* [`write_file_blocking_threads`](src/examples/write_file_blocking_threads.c) - Multiple threads, single connection, blocking operations.
* [`write_file_nonblocking`](src/examples/write_file_nonblocking.c) - Single thread, single connection, multiple non-blocking operations
* [`write_file_blocking_threads`](src/examples/write_file_blocking_threads.c) - Multiple threads, single connection, multiple non-blocking operations.
* [`get_key_range`](src/examples/get_key_range.c) - Query a range of keys from a device.
