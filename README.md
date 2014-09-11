[![Build Status](http://travis-ci.org/atomicobject/kinetic-c.png?branch=master)](http://travis-ci.org/atomicobject/kinetic-c)

Kinetic C Client Library
========================

This repo contains code for producing Kinetic C clients which use the Seagate Kinetic protocol. Code examples are included for reference as part of a utility (`kinetic-c-client-utility`) that builds and links against an installed Kinetic C static library.

The project Makefile can be used as a reference for developing a Makefile for building for a new custom Kinetic C client.

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
    > bundle install # Ensure you have all RubyGems at the proper versions

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

    > make run

API Documentation
=================
[Kinetic-C API](http://seagate.github.io/kinetic-c/kinetic__api_8h.html) (generated with Doxygen)

Examples
========

The following examples are provided for development reference and as utilities to aid development. In order to execute a given example, you must first do:

    > cd build/artifacts/release

You can then execute `kinetic-c-client-util` with a valid example name, optionally preceeded with any of the optional arguments.

**Options**
* `--host [HostName/IP]` or `-h [HostName/IP]` - Set the Kinetic Device host
* `--tls` - Use the TLS port to execute the specified operation(s)

**Operations**
* `kinetic-c-client [--host|-h hostname|123.253.253.23] noop put get`
    * `kinetic-c-client-util noop`
        * Execute a NoOp (ping) operation to verify the Kinetic Device is ready
    * `kinetic-c-client-util put`
        * Execute a Put operation to store a key/value
    * `kinetic-c-client-util get`
        * Execute a Get operation to retrieve a key/value
