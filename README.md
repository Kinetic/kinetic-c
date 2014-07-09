Introduction
============
This repo contains code for producing C Kinetic clients which use the Seagate Kinetic protocol. Code examples/utilities that use the Kinetic C library are included for reference and usage during development.

Cloning the Repo
================
    > git clone https://github.com/atomicobject/kinetic-c.git
    > cd kinetic-c
    > git submodule init
    > git submodule update
    > cd vendor/ceedling
    > git submodule init # Needed since ceedling has its own submodules
    > cd - # go back to the kinetic-c repo root
    > git submodule update --recursive # Updates ALL submodules, including nested ones (ceedling)

    > bundle install # Ensures you have all RubyGems needed

    > rake #run all tests and build kinetic-c library and examples

*NOTE: Once you have performed the above steps, you can get updates to kinetic-c and all nested submodules by simply doing: `git submodule update --recursive`

Protocol Version
=================
The client is using version `2.0.4` of the [Kinetic-Protocol](https://github.com/Seagate/kinetic-protocol/releases/tag/2.0.4).

Dependencies
============
* [Ruby](http://ruby-doc.org) v1.9.3 or higher
    * Required RubyGems
        * bundler 1.3.5 or higher
        * rake 0.9.2.2 or higher
        * require_all
        * constructor
        * diy
* [Ceedling](https://github.com/ThrowTheSwitch/Ceedling) build/test system for C projects
    * Ceedling also includes the following tools for testing C code
        * [Unity](https://github.com/ThrowTheSwitch/Unity) lightweight assertion framework and test executor for C
        * [CMock](https://github.com/ThrowTheSwitch/CMock) mock/fake generator for C modules using only C header files as input (written in Ruby)
* [CppCheck](http://cppcheck.sourceforge.net/) for static analysis source code
* [Valgrind](http://valgrind.org/) for memory tests
* [Doxygen](https://github.com/doxygen) and [GraphViz](http://www.graphviz.org/) for generating documentation

Initial Setup
=============
TBD

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
