v0.10.0 (kinetic-protocol 3.0.5)
-------------------------------
* Added put_nonblocking and get_nonblocking examples
* Added KineticSemaphore API to provide a simpler wrapper around a common use of pthread condition variables as a thread safe way to signal when an async operation has finished
* Switched internal message infrastructure to use a threadpool. This will allow for a much higher number active connections and outstanding commands.
* KineticClient_Init now returns a KineticClient pointer (internally, it's a handle to the threadpool) that must be passed to KineticClient_CreateConnection() in order to create new connections and must also be passed to KineticClient_Shutdown() on shutdown
* Improved I/O examples to demonstrate client write operations for blocking/non-blocking (asynchrounous) and single/multi-threaded.

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
