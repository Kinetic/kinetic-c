Kinetic-C Client Library
========================
![](http://travis-ci.org/atomicobject/kinetic-c.png?branch=master)

This repo contains code for producing C Kinetic clients which use the Seagate Kinetic protocol. Code examples/utilities that use the Kinetic C library are included for reference and usage during development.

[Kinetic-C build status](http://travis-ci.org/atomicobject/kinetic-c) is provided via [Travis CI](http://travis-ci.org)

Prerequisites
-------------
* [Open SSL](https://www.openssl.org/) for security and encryption

Getting Started
---------------

**Clone the repo**

    > git clone --recursive https://github.com/atomicobject/kinetic-c.git
    > cd kinetic-c
    > bundle install # Ensure you have all RubyGems at the proper versions

**Update to the latest version (previously cloned)**

    > git pull
    > git submodule update --init # Ensure submodules are up to date
    > bundle install # Ensure you have all RubyGems at the proper versions

**Build and install static library**

    > make
    > sudo make install

**Build example utility and run tests against Kinetic Device simulator**

    > make run

API Documentation
=================
[Kinetic-C API](http://atomicobject.github.io/kinetic-c/kinetic__api_8h.html) (generated with Doxygen)

Examples
========

The following examples are provided for development reference and as utilities to aid development. In order to execute a given example, you must first do:

```
> cd build/artifacts/release
```

You can then execute `kinetic-c` with a valid example name, optionally preceeded with any of the optional arguments.

**Options**
* `--host [HostName/IP]` or `-h [HostName/IP]` - Set the Kinetic Device host
* `--tls` - Use the TLS port to execute the specified operation(s)

**Operations**
* `kinetic-c-client [--hostname|-h hostname|123.253.253.23] noop put get`
    * `kinetic-c noop`
        * Execute a NoOp (ping) operation to verify the Kinetic Device is ready
    * `kinetic-c put`
        * Execute a Put operation to store a key/value
    * `kinetic-c get`
        * Execute a Get operation to retrieve a key/value
