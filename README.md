[![Build Status](http://travis-ci.org/atomicobject/kinetic-c.png?branch=master)](http://travis-ci.org/atomicobject/kinetic-c)

Kinetic C Client Library
========================

The [Github kinetic-c Git repository](https://github.com/Seagate/kinetic-c) contains code for producing Kinetic C clients for interacting with Kinetic storage object-based storage. The library uses the cross-platform Seagate Kinetic protocol for standardizing interaces between the Java simulator and Kinetic Device storage clusters.

[Code examples](https://github.com/Seagate/kinetic-c/tree/master/src/utility/examples) are included for reference as part of the [kinetic-c client library test utility (`kinetic-c-client-utility`)](https://github.com/Seagate/kinetic-c/tree/master/src/utility), which builds and links against the installed `kinetic-c-client` static library.

The [project Makefile](https://github.com/Seagate/kinetic-c/blob/master/Makefile) can be used as a reference for developing a Makefile-based project for building for a custom Kinetic Storage C client driver and/or a high-level C library.

[Kinetic-C build status](http://travis-ci.org/atomicobject/kinetic-c) is provided via [Travis CI](http://travis-ci.org)

Prerequisites
-------------
* [Open SSL](https://www.openssl.org/) for security and encryption
    * Installation (if you don't already have OpenSSL installed)
        * Linux (using apt-get)
            * `> sudo apt-get install openssl`
        * OSX (using [Homebrew](http://brew.sh/))
            * `> brew install openssl`

Getting Started
---------------

**Clone the repo**

    > git clone --recursive https://github.com/atomicobject/kinetic-c.git
    > cd kinetic-c
    > bundle install # ensure you have all RubyGems at the proper versions

**Update to the latest version (previously cloned)**

    > git pull
    > git submodule update --init # Ensure submodules are up to date
    > bundle install # Ensure you have all RubyGems at the proper versions

**Build and install static library**

    > make
    > sudo make install

**You can all clean and uninstall old versions**

    > make clean
    > sudo uninstall

**Build example utility and run tests against Kinetic Device simulator**

    > make all # this is what Travis-CI build does to ensure it all keeps working

**Uninstall, perform a full clean build with test, and validate with utility**

    > make all # this is what Travis CI does

API Documentation
=================

[Kinetic-C API Documentation](http://seagate.github.io/kinetic-c) (generated with Doxygen)
* [Kinetic-C API](http://seagate.github.io/kinetic-c/kinetic__client_8h.html)
* [Kinetic-C types](http://seagate.github.io/kinetic-c/kinetic__types_8h.html)

Example Client/Test Utility
===========================

Code examples are included for reference as part of a test utility. The source code for the utility is used to build both a static and dynamically linked verion of the kinetic-c-client library.

* 'kinetic-c-util' builds/links against installed Kinetic C static library (.a)
* 'kinetic-c-util.x.y.z' builds/links against installed Kinetic C dynamic library (.so)

The project Makefile can be used as a reference for developing a Makefile for building for a new custom Kinetic C client.

In order to execute a given example, after building it, you must first do:

    > cd bin

You can then execute `kinetic-c-client-util` with a valid example name, optionally preceeded with any of the optional arguments.

Options
-------
* `--host [HostName/IP]` or `-h [HostName/IP]` - Set the Kinetic Device host
* `--tls` - Use the TLS port to execute the specified operation(s)

Operations
----------
* `kinetic-c-client [--host|-h hostname|123.253.253.23] noop put get`
    * `kinetic-c-client-util noop`
        * Execute a NoOp (ping) operation to verify the Kinetic Device is ready
    * `kinetic-c-client-util put`
        * Execute a Put operation to store a key/value entry
    * `kinetic-c-client-util get`
        * Execute a Get operation to retrieve a key/value entry
    * `kinetic-c-client-util delete`
        * Execute a Delete operation to destroy a key/value entry
