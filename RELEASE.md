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
* Added asynchronous/non-blocking execution option to all operations (GET/PUT/DELETE/GETKEYRANGE).
    * A given operation is carried out in asynchronous mode if a closure callback with optional data is supplied.
* ByteArray and ByteBuffer types are now supplied directly in the byte_array.h public interface file.
