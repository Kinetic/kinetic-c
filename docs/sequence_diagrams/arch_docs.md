Architecture Reference
======================

# Dataflow

The request/response dataflow is handled by a multi-threaded messaging bus with single-ownership of messages. When the caller makes a request, the message is put in a `struct boxed_msg`, which will be passed along with a single owner at all times. It is used to track the outgoing request (until it has finished sending), the overall status of the request, the callback that should receive it, and timestamps along the way.

This boxed_msg goes from the Client thread to the Listener thread, then the callback is run with the request's result and/or error status in the thread pool. If the request is invalid (such as a NULL pointer for the payload, or an out-of-order sequence ID), it will be rejected by `bus_send_request`. If the request fails to send (due to a network timeout or hang-up), the box will skip the Listener thread and go directly to running the callback in the threadpool with an appropriate error status.

Socket connections must be registered with the message bus, via `bus_register_socket`. This defines whether they should use SSL or not, and binds other protocol-specific data to them. When a socket is disconnected, `bus_release_socket` will free the internal resources.


# Diagrams

A full request/response cycle flowing through the library.

![](request_response.png?raw=true)


A message going through the message bus, with no errors.

![](msg_handling-no_errors.png?raw=true)


A message going through the message bus, with no reply (a time-out error).

![](msg_handling-no_reply.png?raw=true)


A message going through the message bus, with a hang-up error while sending.

![](msg_handling-send_error.png?raw=true)


# Entities

## Client Thread

This is the thread from the client's caller code for the Kinetic-C library. When sending a request, it is blocked until the request has finished being delivered to an a socket. Before it is unblocked, it may sleep for a small amount of time as backpressure against a busy message handling system.

Before sending a request, the Client thread sends a HOLD message to the Listener thread. This notifies the Listener it that if a response is received for a specific <file descriptor, sequence ID> pair before getting more info about what to do with it, it should hold on to it and await details. (This HOLD process has a timeout equal to the message timeout plus 5 seconds, to cover a window where the request has completed within its timeout, but just barely, and thread scheduling between the Client and Listener threads could lead to the latter timing out without knowing what to do with the response and leaking memory.)

Once the Client has finished sending, it will send an EXPECT message to the Listener thread, transferring the boxed_msg (including the callback), and then unblock. If the EXPECT message cannot be delivered due to a busy Listener, it will retry several times, and then fail with a time out.


## Listener Thread

The Listener thread matches up responses with pending requests and handles unsolicited messages / hang-ups from otherwise idle connections.

If there are going to be lots of concurrent connections (TBD: probably more than several hundred), then the message bus can start multiple Listener threads and assign sockets to them round-robin.

When the Listener receives a response, it will attempt to match it to a pending request and deliver the response and callback to the thread pool. If no associated request is found, the unexpected message callback will be called to notify the client code and free resources.


## Thread Pool

The thread pool allows the response callbacks to be handled via multiple threads, to allow concurrent result processing. A user-provided callback will be called on an arbitrary thread pool thread, with a status code and (if received) an unpacked response.


# Other Concepts

## Sequence ID

A sequence ID is a signed 64-bit integer used to identify messages for a specific connection. They increase monotonically, with gaps allowed (e.g. for messages that are not completely constructed). The message bus will reject messages with sequence IDs <= the last sent, because they will cause the drive to hang up.


## HOLD Message

A HOLD message is used to notify the Listener thread that the client is going to start sending a request, and if a response is received for a given connection (file descriptor) and message (sequence ID), it should be held until further details arrive.


## EXPECT Message

An EXPECT message is used to notify the Listener thread that a request has finished sending for a given connection and message, and to deliver the callback and other details for how to handle it.
