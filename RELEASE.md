v0.5.0
------
* Added blocking PUT operation

v0.6.0
------
* Added blocking GET and DELETE operations.
* Added Makefile build implementing standard (make; sudo make install) interface.
* Added creation/installation of static (.a) and dynamic (.so) libraries
* Added ByteArray type for buffer management.
* Added Kinetic_KeyValue type for key/value pair handling.

v0.7.0
------
* Added blocking GETKEYRANGE operation.

v0.8.0
------
* Updated to use Kinetic Protocol v3.0 (3.0.5)
    * NOT backwards compatible with earlier versions of Kinetic Protocol
    * Kinetic device firmware must be updated to a release supporting v3.0 protocol!
* Added asynchronous/non-blocking execution option to all operations (GET/PUT/DELETE/GETKEYRANGE).
    * A given operation is carried out in asynchronous mode if a closure callback with optional data is supplied.
* ByteArray and ByteBuffer types are now supplied directly in the byte_array.h public interface file.

v0.8.1
------
* Added GETLOG operation.
* Added InstantSecureErase operation.
* Added thread-safety to allow multiple client threads within the same session.
* Still using Kinetic Protocol v3.0 (3.0.5)
    * NOT backwards compatible with earlier versions of Kinetic Protocol (< v3.0)
    * Kinetic device firmware must be updated to a release supporting v3.0 protocol!

v0.8.2
------
* Added FLUSHALLDATA operation.
