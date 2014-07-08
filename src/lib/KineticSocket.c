#include "KineticSocket.h"

int KineticSocket_Connect(const char* host, int port)
{
    return KINETIC_PROTO_STATUS_STATUS_CODE_INVALID_STATUS_CODE;

//     LOG(INFO) << "Connecting to " << host_ << ":" << port_;

//     struct addrinfo hints;
//     memset(&hints, 0, sizeof(struct addrinfo));

//     // could be inet or inet6
//     hints.ai_family = PF_UNSPEC;
//     hints.ai_socktype = SOCK_STREAM;
//     hints.ai_protocol = IPPROTO_TCP;
//     hints.ai_flags = AI_NUMERICSERV;

//     struct addrinfo* result;

//     string port_str = std::to_string(port_);

//     if (int res = getaddrinfo(host_.c_str(), port_str.c_str(), &hints, &result) != 0) {
//         LOG(ERROR) << "Could not resolve host " << host_ << " port " << port_ << ": "
//                 << gai_strerror(res);
//         return false;
//     }

//     struct addrinfo* ai;
//     int socket_fd;
//     for (ai = result; ai != NULL; ai = ai->ai_next) {
//         char host[NI_MAXHOST];
//         char service[NI_MAXSERV];
//         if (int res = getnameinfo(ai->ai_addr, ai->ai_addrlen, host, sizeof(host), service,
//                 sizeof(service), NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
//             LOG(ERROR) << "Could not get name info: " << gai_strerror(res);
//             continue;
//         } else {
//             LOG(INFO) << "Trying to connect to " << string(host) << " on " << string(service);
//         }

//         socket_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

//         if (socket_fd == -1) {
//             LOG(WARNING) << "Could not create socket";
//             continue;
//         }

//         // os x won't let us set close-on-exec when creating the socket, so set it separately
//         int current_fd_flags = fcntl(socket_fd, F_GETFD);
//         if (current_fd_flags == -1) {
//             PLOG(ERROR) << "Failed to get socket fd flags";
//             close(socket_fd);
//             continue;
//         }
//         if (fcntl(socket_fd, F_SETFD, current_fd_flags | FD_CLOEXEC) == -1) {
//             PLOG(ERROR) << "Failed to set socket close-on-exit";
//             close(socket_fd);
//             continue;
//         }

//         // On BSD-like systems we can set SO_NOSIGPIPE on the socket to prevent it from sending a
//         // PIPE signal and bringing down the whole application if the server closes the socket
//         // forcibly
// #ifdef SO_NOSIGPIPE
//         int set = 1;
//         int setsockopt_result = setsockopt(socket_fd, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));
//         // Allow ENOTSOCK because it allows tests to use pipes instead of real sockets
//         if (setsockopt_result != 0 && setsockopt_result != ENOTSOCK) {
//             PLOG(ERROR) << "Failed to set SO_NOSIGPIPE on socket";
//             close(socket_fd);
//             continue;
//         }
// #endif

//         if (connect(socket_fd, ai->ai_addr, ai->ai_addrlen) == -1) {
//             PLOG(WARNING) << "Unable to connect";
//             close(socket_fd);
//             continue;
//         }

//         if (nonblocking_ && fcntl(socket_fd, F_SETFL, O_NONBLOCK) != 0) {
//             PLOG(ERROR) << "Failed to set socket nonblocking";
//             close(socket_fd);
//             continue;
//         }

//         break;
//     }

//     freeaddrinfo(result);

//     if (ai == NULL) {
//         // we went through all addresses without finding one we could bind to
//         LOG(ERROR) << "Could not connect to " << host_ << " on port " << port_;
//         return false;
//     }

//     fd_ = socket_fd;
//     return true;
}
