Kinetic-C Library Developer Reference
=====================================

Prerequisites
-------------

* [Ruby](https://www.ruby-lang.org) (v1.9.3 or higher) scripting language
* [RubyGems](http://rubygems.org) (installed w/ `bundle install)
    * [bundler](http://bundler.io) (v1.3.5 or higher) environment/dependency manager for Ruby projects
* [CppCheck](http://cppcheck.sourceforge.net/) for static analysis of source code
* [Valgrind](http://valgrind.org/) for validation of memory usage/management
* [Doxygen](https://github.com/doxygen) and [GraphViz](http://www.graphviz.org/) for generating API documentation

API Documentation
=================
[Kinetic-C API](http://seagate.github.io/kinetic-c/) (generated with Doxygen)

Common Developer Tasks
----------------------

**NOTE: Prefix the following commands with `bundle exec` so that they execute in the context of the bundle environment setup via `bundle install`.**

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
