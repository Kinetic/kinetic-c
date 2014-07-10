Introduction
============
This repo contains code for producing C Kinetic clients which use the Seagate Kinetic protocol. Code examples/utilities that use the Kinetic C library are included for reference and usage during development.

Continuous integration for kinetic-c on [Travis CI](http://travis-ci.org). [![Build Status](https://travis-ci.org/atomicobject/kinetic-c.png?branch=master)](https://travis-ci.org/atomicobject/kinetic-c)

Cloning the Repo
================
    > git clone --recursive https://github.com/atomicobject/kinetic-c.git

    > cd kinetic-c
    > bundle install # Ensures you have all RubyGems needed
    > rake #run all tests and build kinetic-c library and examples


Protocol Version
=================
The client is using version `2.0.4` of the [Kinetic-Protocol](https://github.com/Seagate/kinetic-protocol/releases/tag/2.0.4).

Dependencies
============
* [Ruby](http://ruby-doc.org) v1.9.3 or higher
* RubyGems (installed w/ `bundle install`)
    * bundler 1.3.5 or higher
    * rake 0.9.2.2 or higher
    * require_all
    * constructor
    * diy
    * [ceedling](https://github.com/ThrowTheSwitch/Ceedling) build/test system for C projects
        * Ceedling also includes the following tools for testing C code
        * [Unity](https://github.com/ThrowTheSwitch/Unity) lightweight assertion framework and test executor for C
        * [CMock](https://github.com/ThrowTheSwitch/CMock) mock/fake generator for C modules using only C header files as input (written in Ruby)
* [CppCheck](http://cppcheck.sourceforge.net/) for static analysis source code
* [Valgrind](http://valgrind.org/) for memory tests
* [Doxygen](https://github.com/doxygen) and [GraphViz](http://www.graphviz.org/) for generating documentation

Common Developer Tasks
======================

* Run all tests and build the library and examples
    * `rake`

* Just build the library
    * `rake release`

* Run all unit/integration tests
    * `rake test:all`

* Analyze code
    * `rake cppcheck`

* Generating documentation
    * TBD

* Apply licenses
    * `./config/apply_license.sh my_new_file.cc` or `./config/apply_license.sh src/lib/*.h`

* Build/install Google Protocol Buffers support for the Kinetic-Protocol
    * `rake proto`

Examples
========

The following examples are provided for development reference and as utilities to aid development

`read_file_blocking` (incomplete!)

`read_file_nonblocking` (incomplete!)

`write_file_blocking` (incomplete!)

`write_file_nonblocking` (incomplete!)

`write_file_blocking_threads` (incomplete!)

`delete_file_blocking` (incomplete!)

`delete_file_nonblocking` (incomplete!)

`instant_secure_erase` (incomplete!)

`kinetic_stat` (incomplete!)

`dump_keyspace` (incomplete!)

`set_acls` (incomplete!)

`set_pin` (incomplete!)

`set_clusterversion` (incomplete!)

`update_firmware` (incomplete!)

`copy_drive` (incomplete!)
