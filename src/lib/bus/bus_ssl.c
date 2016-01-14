/**
 * Copyright 2013-2015 Seagate Technology LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla
 * Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at
 * https://mozilla.org/MP:/2.0/.
 *
 * This program is distributed in the hope that it will be useful,
 * but is provided AS-IS, WITHOUT ANY WARRANTY; including without
 * the implied warranty of MERCHANTABILITY, NON-INFRINGEMENT or
 * FITNESS FOR A PARTICULAR PURPOSE. See the Mozilla Public
 * License for more details.
 *
 * See www.openkinetic.org for more project information
 */

#include <poll.h>
#include <assert.h>

#include "bus_ssl.h"
#include "syscall.h"
#include "util.h"

#define TIMEOUT_MSEC 100
#define MAX_TIMEOUT 10000

static bool init_client_SSL_CTX(SSL_CTX **ctx_out);
static void disable_SSL_compression(void);
static void disable_known_bad_ciphers(SSL_CTX *ctx);
static bool do_blocking_connection(struct bus *b, SSL *ssl, int fd);

/* Initialize the SSL library internals for use by the messaging bus. */
bool BusSSL_Init(struct bus *b) {
    if (!SSL_library_init()) { return false; }
    SSL_load_error_strings();
    ERR_load_BIO_strings();
    OpenSSL_add_ssl_algorithms();

    SSL_CTX *ctx = NULL;
    if (!init_client_SSL_CTX(&ctx)) { return false; }
    b->ssl_ctx = ctx;

    return true;
}

/* Do an SSL / TLS handshake for a connection. Blocking.
 * Returns whether the connection succeeded. */
SSL *BusSSL_Connect(struct bus *b, int fd) {
    SSL *ssl = NULL;

    ssl = SSL_new(b->ssl_ctx);
    if (ssl == NULL) {
        ERR_print_errors_fp(stderr);
        return NULL;
    }

    if (!SSL_set_fd(ssl, fd)) {
        return NULL;
    }

    if (do_blocking_connection(b, ssl, fd)) {
        return ssl;
    } else {
        SSL_free(ssl);
        return NULL;
    }
}

/* Disconnect and free an individual SSL handle. */
bool BusSSL_Disconnect(struct bus *b, SSL *ssl) {
    SSL_free(ssl);
    (void)b;
    return true;
}

/* Free all internal data for using SSL (the SSL_CTX). */
void BusSSL_CtxFree(struct bus *b) {
    if (b && b->ssl_ctx) {
        SSL_CTX_free(b->ssl_ctx);
        b->ssl_ctx = NULL;
    }
}

static bool init_client_SSL_CTX(SSL_CTX **ctx_out) {
    SSL_CTX *ctx = NULL;
    assert(ctx_out);

    /* Create TLS context */
    const SSL_METHOD *method = NULL;

    #if KINETIC_USE_TLS_1_2
        method = TLSv1_2_client_method();
    #else
        method = TLSv1_1_client_method();
    #endif

    assert(method);
    ctx = SSL_CTX_new(method);
    if (ctx == NULL) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    disable_SSL_compression();
    disable_known_bad_ciphers(ctx);
    *ctx_out = ctx;
    return true;
}

static void disable_SSL_compression(void) {
    STACK_OF(SSL_COMP) *ssl_comp_methods;
    ssl_comp_methods = SSL_COMP_get_compression_methods();
    int n = sk_SSL_COMP_num(ssl_comp_methods);
    for (int i = 0; i < n; i++) {
        (void) sk_SSL_COMP_pop(ssl_comp_methods);
    }
}

static void disable_known_bad_ciphers(SSL_CTX *ctx) {
    const char CIPHER_LIST_CFG[] =
      "ECDH+AESGCM:DH+AESGCM:ECDH+AES256:DH+AES256:ECDH+AES128:DH+AES:"
      "ECDH+3DES:DH+3DES:RSA+AESGCM:RSA+AES:RSA+3DES:!aNULL:!MD5:!DSS";
    int res = SSL_CTX_set_cipher_list(ctx, CIPHER_LIST_CFG);
    assert(res == 1);
}

static bool do_blocking_connection(struct bus *b, SSL *ssl, int fd) {
    BUS_LOG_SNPRINTF(b, 2, LOG_SOCKET_REGISTERED, b->udata, 128,
        "SSL_Connect handshake for socket %d", fd);

    struct pollfd fds[1];
    fds[0].fd = fd;
    fds[0].events = POLLOUT;

    bool connected = false;
    size_t elapsed = 0;

    while (!connected) {
        int pres = syscall_poll(fds, 1, TIMEOUT_MSEC);
        BUS_LOG_SNPRINTF(b, 5, LOG_SOCKET_REGISTERED, b->udata, 128,
            "SSL_Connect handshake for socket %d, poll res %d", fd, pres);

        if (pres < 0) {
            if (Util_IsResumableIOError(errno)) {
                errno = 0;
            } else {
                /*  */
                assert(false);
            }
        } else if (pres > 0) {
            if (fds[0].revents & (POLLOUT | POLLIN)) {
                int connect_res = SSL_connect(ssl);
                BUS_LOG_SNPRINTF(b, 5, LOG_SOCKET_REGISTERED, b->udata, 128,
                    "socket %d: connect_res %d", fd, connect_res);

                if (connect_res == 1) {
                    BUS_LOG_SNPRINTF(b, 5, LOG_SOCKET_REGISTERED, b->udata, 128,
                        "socket %d: successfully connected", fd);
                    connected = true;
                } else if (connect_res < 0) {
                    int reason = SSL_get_error(ssl, connect_res);

                    switch (reason) {
                    case SSL_ERROR_WANT_WRITE:
                        BUS_LOG(b, 4, LOG_SOCKET_REGISTERED, "WANT_WRITE", b->udata);
                        fds[0].events = POLLOUT;
                        break;

                    case SSL_ERROR_WANT_READ:
                        BUS_LOG(b, 4, LOG_SOCKET_REGISTERED, "WANT_READ", b->udata);
                        fds[0].events = POLLIN;
                        break;

                    case SSL_ERROR_SYSCALL:
                    {
                        if (Util_IsResumableIOError(errno)) {
                            errno = 0;
                            break;
                        } else {
                            unsigned long errval = ERR_get_error();
                            char ebuf[256];
                            BUS_LOG_SNPRINTF(b, 1, LOG_SOCKET_REGISTERED, b->udata, 128,
                                "socket %d: ERROR -- %s", fd, ERR_error_string(errval, ebuf));
                        }
                    }
                    break;
                    default:
                    {
                        unsigned long errval = ERR_get_error();
                        char ebuf[256];
                        BUS_LOG_SNPRINTF(b, 1, LOG_SOCKET_REGISTERED, b->udata, 128,
                            "socket %d: ERROR %d -- %s", fd, reason, ERR_error_string(errval, ebuf));
                        assert(false);
                    }
                    }

                } else {
                    BUS_LOG_SNPRINTF(b, 5, LOG_SOCKET_REGISTERED, b->udata, 128,
                        "socket %d: unknown state, setting event bask to (POLLIN | POLLOUT)",
                        fd);
                    fds[0].events = (POLLIN | POLLOUT);
                }
            } else if (fds[0].revents & POLLHUP) {
                BUS_LOG_SNPRINTF(b, 1, LOG_SOCKET_REGISTERED, b->udata, 128,
                    "SSL_Connect: HUP on %d", fd);
                return false;
            } else if (fds[0].revents & POLLERR) {
                BUS_LOG_SNPRINTF(b,12, LOG_SOCKET_REGISTERED, b->udata, 128,
                    "SSL_Connect: ERR on %d", fd);
                return false;
            }
        } else {
            BUS_LOG(b, 4, LOG_SOCKET_REGISTERED, "poll timeout", b->udata);
            elapsed += TIMEOUT_MSEC;
            if (elapsed > MAX_TIMEOUT) {
                BUS_LOG(b, 2, LOG_SOCKET_REGISTERED, "timed out", b->udata);
                return false;
            }
        }
    }

    return connected;
}
