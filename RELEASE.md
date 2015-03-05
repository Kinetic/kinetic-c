v0.12.0 (kinetic-protocol 3.0.5)
--------------------------------
* `KineticSession` is now supplied as an opaque instance pointer from `KineticClient_CreateSession`
    * Passed `KineticClientConfig` is now deep copied and can be discarded after session creation.
* Updated `KineticClient_Put` to allow storing an object with an empty value with a NULL data pointer supplied.
* Added new `KineticAdminClient` API (see `include/kinetic_admin_client.h`)
    * Reloacted existing admin methods to admin API
        * `KineticClient_InstantSecureErase` => `KineticAdminClient_InstantErase`/`KineticAdminClient_SecureErase`
        * `KineticClient_GetLog` => `KineticAdminClient_GetLog`/`KineticAdminClient_GetDeviceSpecificLog`
        * `KineticClient_FreeDeviceInfo` => `KineticAdminClient_LogInfo`
* Updated `kinetic-c-util` to support new Admin API. Still need to add support for printing info returned from `KineticAdminClient_GetLog`
* Changed `kinetic-c-util` API to use `getopt_long` for operations in addition to parameters and added usage info via `--help`/`-?` option.
* Fixed potential memory leak when logging protocol buffers.
* Fixed potential imbalance in concurrent operations semaphore.
* Status code changes
    * Changed
        * `KINETIC_STATUS_HMAC_EMPTY` => `KINETIC_STATUS_HMAC_REQUIRED`
    * Added
        * `KINETIC_STATUS_MISSING_PIN`
        * `KINETIC_STATUS_SSL_REQUIRED`
        * `KINETIC_STATUS_DEVICE_LOCKED`
        * `KINETIC_STATUS_ACL_ERROR`
        * `KINETIC_STATUS_NOT_AUTHORIZED`
        * `KINETIC_STATUS_INVALID_FILE`
        * `KINETIC_STATUS_INVALID_LOG_TYPE`
* Set larger timeouts for operations that tend to take approx. 10 seconds, to prevent non-deterministic failures.
* API change: Eliminated KineticClientConfig.writerThreads, since sender threads are gone.

v0.11.2 (kinetic-protocol 3.0.5)
--------------------------------
* Changed all threads to block indefinitely and be fully event-driven.
* Added logging of all assertions to provide debugging info in the absence of debug symbols.
* Fixed race condition in message bus listener which could lead to data corruption.
* Fixed race condition that could cause more than the max number of thread pool threads to be created.
* Fixed potential buffer overrun in logger.
* Fixed bug with mishandling of unsolicited status responses from the drive prior to drive terminating connection.
* Removed ruby/rake as non-development build prerequisites.
* Added make json, json_install and json_uninstall tasks for json-c.

v0.11.1 (kinetic-protocol 3.0.5)
--------------------------------
* Fixed race condition causing timeouts to not be handled resulting in a deadlock.
* Fixed extremely high CPU usage during loaded and idle times.

v0.11.0 (kinetic-protocol 3.0.5)
--------------------------------
* Changed API to use a `KineticClientConfig` struct, to keep future configuration changes from breaking the source API.
    * NOTE: KineticClientConfig and KineticSessionConfig rely on C99 struct init.
        * Unconfigured fields will be reverted to defaults if set to 0.
        * These structures must be configured via C99 struct init or memset to 0 prior to population for backwards compatibility.
* Added options for the number of writer, reader, and max threadpool threads, with defaults.
* Added KineticClient_FreeDeviceInfo to free the `KineticDeviceInfo` structure allocated by `KineticClient_GetLog`.
* Added several new examples under src/examples/.
* Added json-c library dependency which will be used for JSON-formatted ACL files for upcoming Admin API and multi-cast drive discovery tool.

v0.10.1 (kinetic-protocol 3.0.5)
--------------------------------
* Fixed regression in 0.10.0 where a GET with metadataOnly=true would result in a crash if a value field was not provided
* Added system tests for P2P operations which are now fully validated and working on HW
* Fixed misue of pthread_join status in thread pool which was causing shutdown issues in GCC builds

v0.10.0 (kinetic-protocol 3.0.5)
--------------------------------
* Added put_nonblocking and get_nonblocking examples
* Added KineticSemaphore API to provide a simpler wrapper around a common use of pthread condition variables as a thread safe way to signal when an async operation has finished
* Switched internal message infrastructure to use a threadpool. This will allow for a much higher number active connections and outstanding commands.
* KineticClient_Init now returns a KineticClient pointer (internally, it's a handle to the threadpool) that must be passed to KineticClient_CreateConnection() in order to create new connections and must also be passed to KineticClient_Shutdown() on shutdown
* Improved I/O examples to demonstrate client write operations for blocking/non-blocking (asynchrounous) and single/multi-threaded.
* Added consolidated performance test which reports metrics on PUT/GET/DELETE sequences (test/system/test_system_async_throughput.c).

v0.9.2 (kinetic-protocol 3.0.5)
-------------------------------
* Added missing mutex lock causing a concurrency issue.
* Added profiling to test/system/test_system_async_io.c

v0.9.1 (kinetic-protocol 3.0.5)
-------------------------------
* Added get_key_range.c example for KineticClient_GetKeyRange().

v0.9.0 (kinetic-protocol 3.0.5)
-------------------------------
* Changed API to use a `KineticSession` with a pointer to private session/connection info in place of the old `KineticSessionHandle`.
* Added `FLUSHALLDATA` operation via `KineticClient_Flush()`
* Added `GETPREVIOUS`/`GETNEXT` operations via `KineticClient_GetPrevious()`/`KineticClient_GetNext()`
* Replaced `KinticClient_Connect()`/`Disconnect()` with `KineticClient_CreateConnection()`/`DestroyConnection()`, since the connection structure is dynamically allocated and populated in the provided `KineticSession`. `KineticClient_DestroyConnection()` MUST be called on the an established `KineticSession` in order to shutdown the connection and free up resources.
* Added initial implementation of PEER2PEERPUSH which does NOT yet support chaining via KineticClient_P2POperation().
* Fixed bug with mutex locking causing an assert failure and subsequent crash.
* Fixed leaking of destroyed connections and they are now properly freed upon call to KineticClient_DestoryConnection().
* Fixed bug where some unrecoverable socket errors could cause retries when unintended.
* Modified PDU flow control in order not stall transmission on request PDUs when Value payload is empty or of small size.

v0.8.1 (kinetic-protocol 3.0.5)
-------------------------------
* Added `GETLOG` operation.
* Added thread-safety to allow multiple client threads within the same session.
* Added I/O examples to demonstrate client write operations for blocking/non-blocking (asynchrounous) and single/multi-threaded.

v0.8.0 (kinetic-protocol 3.0.5)
-------------------------------
* **Updated to use Kinetic Protocol v3.0 (3.0.5)**
    * **NOT backwards compatible with earlier versions of Kinetic Protocol**
    * **Kinetic device firmware must be updated to a release supporting v3.0 protocol!**
* Added asynchronous/non-blocking execution option to all operations (`GET`/`PUT`/`DELETE`/`GETKEYRANGE`).
    * A given operation is carried out in asynchronous mode if a closure callback with optional data is supplied.
* `ByteArray` and `ByteBuffer` types are now supplied directly in the `byte_array.h` public interface file.
* Fixed a concurrency issue that intermittently produces a crash in `kinetic_allocator` in a threaded context

v0.7.0 (kinetic-protocol 2.0.6)
-------------------------------
* Added blocking `GETKEYRANGE` operation.

v0.6.0 (kinetic-protocol 2.0.6)
-------------------------------
* Added blocking `GET` and `DELETE` operations.
* Added Makefile build implementing standard (make; sudo make install) interface.
* Added creation/installation of static (.a) and dynamic (.so) libraries
* Added `ByteArray` type for buffer management.
* Added `Kinetic_KeyValue` type for key/value pair handling.

v0.5.0 (kinetic-protocol 2.0.4)
-------------------------------
* Added blocking `PUT` operation
