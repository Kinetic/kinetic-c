Introduction
============
This repo contains code for producing C kinetic clients.

Protocol Version
=================
The client is using version `2.0.4` of the [Kinetic-Protocol](https://github.com/Seagate/kinetic-protocol/releases/tag/2.0.4).

Dependencies
============
* CMake??/Ceedling??
* Doxygen/graphviz for generating documentation

Initial Setup
=============
TBD

Common Developer Tasks
======================

**Building the lib**: TBD

**Running tests**: TBD

There is also an integration test suite. This suite reads the environment
variable `KINETIC_PATH` to determine a simulator executable to run tests
against. If that variable is not set, it instead assumes that a Kinetic server
is running on port 8123 on `localhost`. To run the integration tests, set
`KINETIC_PATH` if appropriate and run `make integration_test`. This will write
a JUnit report to `integrationresults.xml`.

**Checking code style**: TBD

**Generating documentation**: TBD

**Apply licenses**: Run something like `./apply_license.sh my_new_file.c` or `./apply_license.sh include/*.h`
