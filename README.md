Kinetic-C Client Library
========================
![](http://travis-ci.org/atomicobject/kinetic-c.png?branch=master)

This repo contains code for producing C Kinetic clients which use the Seagate Kinetic protocol. Code examples/utilities that use the Kinetic C library are included for reference and usage during development.

[Kinetic-C build status](http://travis-ci.org/atomicobject/kinetic-c) is provided via [Travis CI](http://travis-ci.org)

Getting Started
================
**Clone the repo**
    > git clone --recursive https://github.com/atomicobject/kinetic-c.git
    > cd kinetic-c
    > bundle install # Ensure you have all RubyGems at the proper versions

**Update to the latest version (previously cloned)**
    > git pull
    > git submodule update --init # Ensure submodules are up to date
    > bundle install # Ensure you have all RubyGems at the proper versions

**Build Kinetic-C and run all tests**
    > bundle exec rake

API Documentation
=================
[Kinetic-C API](http://atomicobject.github.io/kinetic-c/) (generated with Doxygen)

Dependencies
============
* [Kinetic Protocol](https://github.com/Seagate/kinetic-protocol)
    * [Kinetic-Protocol v2.0.4](https://github.com/Seagate/kinetic-protocol/releases/tag/2.0.4)
* [Ruby](https://www.ruby-lang.org) (v1.9.3 or higher) scripting language
* [RubyGems](http://rubygems.org) (installed w/ `bundle install)
    * [bundler](http://bundler.io) (v1.3.5 or higher) environment/dependency manager for Ruby projects
    * [rake](https://github.com/jimweirich/rake) (0.9.2.2 or higher) Ruby implementation of Make
    * [ceedling](https://github.com/ThrowTheSwitch/Ceedling) Ruby/Rake-based build/test system for C projects
        * Ceedling also includes the following tools for testing C code
            * [Unity](https://github.com/ThrowTheSwitch/Unity) lightweight assertion framework and test executor for C
            * [CMock](https://github.com/ThrowTheSwitch/CMock) mock/fake generator for C modules using only C header files as input (written in Ruby)
* [CppCheck](http://cppcheck.sourceforge.net/) for static analysis of source code
* [Valgrind](http://valgrind.org/) for validation of memory usage/management
* [Doxygen](https://github.com/doxygen) and [GraphViz](http://www.graphviz.org/) for generating API documentation

Common Developer Tasks
======================

NOTE: Prefix the following commands with `bundle exec` so that they execute in the context of the bundle environment setup via `bundle install`.

* List available Rake tasks w/descriptions
    * `rake -T`
* Run all tests and build the library and examples
    * `rake`
* Build the library
    * `rake release`
* Run all unit/integration/system/example tests
    * `rake test_all`
* Analyze code
    * `rake cppcheck`
* Generate documentation
    * `rake doxygen:gen`
* Apply license to source files (skips already licensed files)
    * `rake apply_license`
* Build/install Google Protocol Buffers support for the Kinetic-Protocol
    * `rake proto`
* Enable verbose output (for current Rake run)
    * `rake verbose <task_A> <task_B>`

Examples
========

The following examples are provided for development reference and as utilities to aid development. In order to execute a given example, you must first do:

```
> cd build/artifacts/release`
```

You can then execute `kinetic-c` with a valid example name, optionally preceeded with any of the optional arguments.

**Options**
* `--host [HostName/IP]` or `-h [HostName/IP]` - Set the Kinetic Device host
* `--tls` - Use the TLS port to execute the specified operation(s)

**Operations**
* `kinetic-c [--hostname|-h hostname|123.253.253.23] noop`
    * `kinetic-c noop`
        * Execute the NoOp operation against default host: `localhost`
    * `kinetic-c noop --host my-kinetic-server.com`
        * Execute the NoOp operation against `my-kinetic-server.com`
* `read_file_blocking` (incomplete!)
* `read_file_nonblocking` (incomplete!)
* `write_file_blocking` (incomplete!)
* `write_file_nonblocking` (incomplete!)
* `write_file_blocking_threads` (incomplete!)
* `delete_file_blocking` (incomplete!)
* `delete_file_nonblocking` (incomplete!)
* `instant_secure_erase` (incomplete!)
* `kinetic_stat` (incomplete!)
* `dump_keyspace` (incomplete!)
* `set_acls` (incomplete!)
* `set_pin` (incomplete!)
* `set_clusterversion` (incomplete!)
* `update_firmware` (incomplete!)
* `copy_drive` (incomplete!)
