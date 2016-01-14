Kinetic-C Library Developer Reference
=====================================

Prerequisites
-------------
* [Ruby](https://www.ruby-lang.org) (v1.9.3 or higher) scripting language
* [RubyGems](http://rubygems.org) (installed w/ `bundle install)
    * [bundler](http://bundler.io) (v1.3.5 or higher) environment/dependency manager for Ruby projects
    * Managed gems (installed w/ `bundle install`)
        * [rake](https://rubygems.org/gems/rake) Make-like build system
        * [ceedling](https://github.com/ThrowTheSwitch/Ceedling) Build system which extends rake to add support for interaction-based testing
            * [unity](https://github.com/ThrowTheSwitch/Unity) Unity C test framework (bundled w/ Ceedling)
            * [cmock](https://github.com/ThrowTheSwitch/CMock) CMock auto mock/fake module generator for C (bundle w/ Ceedling)
* [Valgrind](http://valgrind.org/) for validation of memory usage/management
* [Doxygen](https://github.com/doxygen)
    * [GraphViz](http://www.graphviz.org/) used by Doxygen for generating visualizations/graphs

API Documentation
-----------------
[Kinetic-C API Documentation](http://seagate.github.io/kinetic-c/) (generated with Doxygen)
* [Kinetic-C API](http://seagate.github.io/kinetic-c/kinetic__client_8h.html)
* [Kinetic-C types](http://seagate.github.io/kinetic-c/kinetic__types_8h.html)
* [ByteArray API](http://seagate.github.io/kinetic-c/byte__array_8h.html)
    * The ByteArray and ByteBuffer types are used for exchanging variable length byte-arrays with kinetic-c
        * e.g. object keys, object value data, etc.

There are also some additional [architectural notes](docs) and time-sequence diagrams.

[Architectural Document](docs/sequence_diagrams/arch_docs.md)

Common Developer Tasks
----------------------
* Build the library
    * `make`
* Start kinteic-java simulator(s)
    * `make [NUM_SIMS=N] start_sims # defaults to starting 2 simulators, if NUM_SIMS unspecified`
* Stop all running kinetic-java simulator(s)
    * `make stop_sims`
* Run all tests and build the library and examples
    * `make all`
* Run all unit/integration tests
    * `make test`
* Run all system tests
    * `make system_tests`
* Run a particular system test
    * make test_system_<module>
    * Will expect at least 2 simulators running by default (see above)
    * Uses the following environment vars, which are loaded dynamically at runtime
        * `KINTEIC_HOST[1|2]` - Configures the host name/IP for the specified device (default: `localhost`)
        * `KINTEIC_PORT[1|2]` - Configures the primary port for the specified device (default: `8124`, `8124`)
        * `KINTEIC_TLS_PORT[1|2]` - Configures the TLS port for the specified device (default: `8443`, `8444`)
* Apply license to source files (skips already licensed files)
    * `make apply_license`

Developer Tasks via Rake
------------------------
* List rake tasks w/ descriptions
    * `rake -T`
* Test a single module (via Ceedling)
    * `rake test:<module_name>`
* Generate API documentation locally
    * `rake doxygen:gen`
* Generate and publish public API documentation
    * `rake doxygen:update_public_api`
* Build/install Google Protocol Buffers support for the Kinetic-Protocol
    * `rake proto`

Automated Tests
---------------
All test sources are located in `test/`, which are additionally broken up into:
    * `test/unit` - test suites for individual modules
    * `test/integration` - test suites which integrate multiple modules
    * `test/system` - system tests whick link against the kinetic-c release library
        * These tests require at least 2 simulator/drives to run against

Adding a new unit/integration test
----------------------------------
* Create a file named `test_<name>.c` in either `test/unit` or `test/integration`
    * These files will automatically be picked up by the Ceedling and added to the regression suite
    * Build targets for each module will be generated according to what header files are included in the test suite source.
        * e.g.
            * `#include "kinetic_session.h"` will link `kinetic_session.o` into the test target.
            * `#include "mock_kinetic_session.h"` will create a CMock mock of `kinetic_session.h` and link `mock_kinetic_session.o` into the test target.
            * `#include <some_lib.h>` will simply include the specified header, assuming it is already available in to the linker.

Adding a new system test
------------------------
* Create a file named `test_system_<name>.c` in `test/system`
    * This will create a new system test target invokable via: `make test_system_<name>`
    * System tests link/run against the full kinetic-c static library.
    * A generic test fixure is provided and linked into each system test from: `test/support/system_test_fixture.h/c`
        * See details above for runtime configuration of the system test kinetic devices for running remote simulators or kinetic device hardware.


Future development notes
------------------------

* epoll(2) could be used internally, in place of poll(2) and multiple
  listener threads. This only matters in a case where there is a large
  amount of idle listener connections. When there is a small number of
  file descriptors, it will add overhead, and epoll is only available
  on Linux.

* The listener can potentially leak memory on shutdown, in the case
  where responses have been partially received. This has been a low priority.

* There is room for tuning the total number of messages-in-flight
  in the listener (controlled by `MAX_PENDING_MESSAGES`), how the
  backpressure is calculated (in `ListenerTask_GetBackpressure`), and the
  bit shift applied to the backpressure unit (the third argument to
  `bus_backpressure_delay`, e.g. `LISTENER_BACKPRESSURE_SHIFT`). These
  derive the feedback that pushes against actions that overload the
  system. The current setup has worked well with system/integration
  tests and a stress test program that attempts to overload the message
  bus over a loopback connection, but other workloads may have different
  performance trade-offs.
