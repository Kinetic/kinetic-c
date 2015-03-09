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

Common Developer Tasks
----------------------
* Build the library
    * `make`
* Run all tests and build the library and examples
    * `make all`
* Run all unit/integration tests
    * `make test`
* Run all system tests
    * `make system_tests`
* Apply license to source files (skips already licensed files)
    * `make apply_license`

Developer Tasks via Rake
------------------------
* Generate API documentation
    * `rake doxygen:gen`
* Generate and publish public API documentation
    * `rake doxygen:update_public_api`
* Build/install Google Protocol Buffers support for the Kinetic-Protocol
    * `rake proto`
