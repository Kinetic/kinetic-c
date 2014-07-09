Introduction
============
This repo contains code for producing C kinetic clients.

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

**Building the lib**: TBD
`rake release`

**Running tests**
`rake test:all`

There is also an integration test suite. This suite reads the environment
variable `KINETIC_PATH` to determine a simulator executable to run tests
against. If that variable is not set, it instead assumes that a Kinetic server
is running on port 8123 on `localhost`. To run the integration tests, set
`KINETIC_PATH` if appropriate and run `make integration_test`. This will write
a JUnit report to `integrationresults.xml`.

**Checking code style**
`rake cppcheck`

**Generating documentation**
TBD

**Apply licenses**
`./config/apply_license.sh my_new_file.cc` or `./config/apply_license.sh src/lib/*.h`

**Build/install Google Protocol Buffers support for the Kinetic-Protocol**
`rake proto`

Examples
========

The following examples are provided for development reference and as utilities to aid development

`read_file_blocking` (incomplete!)
------------------------------------------------------

`read_file_nonblocking` (incomplete!)
------------------------------------------------------

`write_file_blocking` (incomplete!)
------------------------------------------------------

`write_file_nonblocking` (incomplete!)
------------------------------------------------------

`write_file_blocking_threads` (incomplete!)
------------------------------------------------------

`delete_file_blocking` (incomplete!)
------------------------------------------------------

`delete_file_nonblocking` (incomplete!)
------------------------------------------------------

`instant_secure_erase` (incomplete!)
------------------------------------------------------

`kinetic_stat` (incomplete!)
------------------------------------------------------

`dump_keyspace` (incomplete!)
------------------------------------------------------

`set_acls` (incomplete!)
------------------------------------------------------

`set_pin` (incomplete!)
------------------------------------------------------

`set_clusterversion` (incomplete!)
------------------------------------------------------

`update_firmware` (incomplete!)
------------------------------------------------------

`copy_drive` (incomplete!)
------------------------------------------------------

