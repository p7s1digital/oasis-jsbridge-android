/*
 *  Example debug transport using a Linux/Unix TCP socket
 *
 *  Provides a TCP server socket which a debug client can connect to.
 *  After that data is just passed through.
 *
 *  On some UNIX systems poll() may not be available but select() is.
 *  The default is to use poll(), but you can switch to select() by
 *  defining USE_SELECT.  See https://daniel.haxx.se/docs/poll-vs-select.html.
 */

#include "log.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#if !defined(USE_SELECT)
#include <poll.h>
#endif  /* !USE_SELECT */
#include <errno.h>
#include "duktape.h"
#include "duk_trans_socket.h"

#if !defined(DUK_DEBUG_PORT)
#define DUK_DEBUG_PORT 9091
#endif

#if 0
#define DEBUG_PRINTS
#endif

static int server_sock = -1;
static int client_sock = -1;

/*
 *  Transport init and finish
 */

void duk_trans_socket_init(void) {
	struct sockaddr_in addr;
	int on;

	server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sock < 0) {
		alog("%s: failed to create server socket: %s",
		        __FILE__, strerror(errno));
		goto fail;
	}

	on = 1;
	if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char *) &on, sizeof(on)) < 0) {
		alog("%s: failed to set SO_REUSEADDR for server socket: %s",
		        __FILE__, strerror(errno));
		goto fail;
	}

	memset((void *) &addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(DUK_DEBUG_PORT);

	if (bind(server_sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		alog("%s: failed to bind server socket: %s",
		        __FILE__, strerror(errno));
		goto fail;
	}

    alog("Before listening server socket");
	listen(server_sock, 1 /*backlog*/);
    alog("After listening server socket");
	return;

 fail:
	if (server_sock >= 0) {
		(void) close(server_sock);
		server_sock = -1;
	}
}

void duk_trans_socket_finish(void) {
    alog("duk_trans_socket_finish()");

	if (client_sock >= 0) {
        shutdown(client_sock, 2);
		(void) close(client_sock);
		client_sock = -1;
	}

    if (server_sock >= 0) {
        shutdown(server_sock, 2);
        (void) close(server_sock);
        server_sock = -1;
    }
}

void duk_trans_socket_waitconn(void) {
	struct sockaddr_in addr;
	socklen_t sz;

	if (server_sock < 0) {
		alog("%s: no server socket, skip waiting for connection",
		        __FILE__);
		return;
	}
	if (client_sock >= 0) {
		(void) close(client_sock);
		client_sock = -1;
	}

	alog("Waiting for debug connection on port %d", (int) DUK_DEBUG_PORT);

	sz = (socklen_t) sizeof(addr);
	client_sock = accept(server_sock, (struct sockaddr *) &addr, &sz);
	if (client_sock < 0) {
		alog("%s: accept() failed, skip waiting for connection: %s",
		        __FILE__, strerror(errno));
		goto fail;
	}

	alog("Debug connection established");

	/* XXX: For now, close the listen socket because we won't accept new
	 * connections anyway.  A better implementation would allow multiple
	 * debug attaches.
	 */

	if (server_sock >= 0) {
		(void) close(server_sock);
		server_sock = -1;
	}
	return;

 fail:
	if (client_sock >= 0) {
		(void) close(client_sock);
		client_sock = -1;
	}
}

/*
 *  Duktape callbacks
 */

/* Duktape debug transport callback: (possibly partial) read. */
duk_size_t duk_trans_socket_read_cb(void *udata, char *buffer, duk_size_t length) {
	ssize_t ret;

	(void) udata;  /* not needed by the example */

#if defined(DEBUG_PRINTS)
	alog("%s: udata=%p, buffer=%p, length=%ld",
	        __func__, (void *) udata, (void *) buffer, (long) length);
#endif

	if (client_sock < 0) {
		return 0;
	}

	if (length == 0) {
		/* This shouldn't happen. */
		alog("%s: read request length == 0, closing connection",
		        __FILE__);
		goto fail;
	}

	if (buffer == NULL) {
		/* This shouldn't happen. */
		alog("%s: read request buffer == NULL, closing connection",
		        __FILE__);
		goto fail;
	}

	/* In a production quality implementation there would be a sanity
	 * timeout here to recover from "black hole" disconnects.
	 */

	ret = read(client_sock, (void *) buffer, (size_t) length);
	if (ret < 0) {
		alog("%s: debug read failed, closing connection: %s",
		        __FILE__, strerror(errno));
		goto fail;
	} else if (ret == 0) {
		alog("%s: debug read failed, ret == 0 (EOF), closing connection",
		        __FILE__);
		goto fail;
	} else if (ret > (ssize_t) length) {
		alog("%s: debug read failed, ret too large (%ld > %ld), closing connection",
		        __FILE__, (long) ret, (long) length);
		goto fail;
	}

	return (duk_size_t) ret;

 fail:
	if (client_sock >= 0) {
		(void) close(client_sock);
		client_sock = -1;
	}
	return 0;
}

/* Duktape debug transport callback: (possibly partial) write. */
duk_size_t duk_trans_socket_write_cb(void *udata, const char *buffer, duk_size_t length) {
	ssize_t ret;

	(void) udata;  /* not needed by the example */

#if defined(DEBUG_PRINTS)
	alog("%s: udata=%p, buffer=%p, length=%ld",
	        __func__, (void *) udata, (const void *) buffer, (long) length);
#endif

	if (client_sock < 0) {
		return 0;
	}

	if (length == 0) {
		/* This shouldn't happen. */
		alog("%s: write request length == 0, closing connection",
		        __FILE__);
		goto fail;
	}

	if (buffer == NULL) {
		/* This shouldn't happen. */
		alog("%s: write request buffer == NULL, closing connection",
		        __FILE__);
		goto fail;
	}

	/* In a production quality implementation there would be a sanity
	 * timeout here to recover from "black hole" disconnects.
	 */

	ret = write(client_sock, (const void *) buffer, (size_t) length);
	if (ret <= 0 || ret > (ssize_t) length) {
		alog("%s: debug write failed, closing connection: %s",
		        __FILE__, strerror(errno));
		goto fail;
	}

	return (duk_size_t) ret;

 fail:
	if (client_sock >= 0) {
		(void) close(client_sock);
		client_sock = -1;
	}
	return 0;
}

duk_size_t duk_trans_socket_peek_cb(void *udata) {
#if defined(USE_SELECT)
	struct timeval tm;
	fd_set rfds;
	int select_rc;
#else
	struct pollfd fds[1];
	int poll_rc;
#endif

	(void) udata;  /* not needed by the example */

#if defined(DEBUG_PRINTS)
	alog("%s: udata=%p", __func__, (void *) udata);
#endif

	if (client_sock < 0) {
		return 0;
	}
#if defined(USE_SELECT)
	FD_ZERO(&rfds);
	FD_SET(client_sock, &rfds);
	tm.tv_sec = tm.tv_usec = 0;
	select_rc = select(client_sock + 1, &rfds, NULL, NULL, &tm);
	if (select_rc == 0) {
		return 0;
	} else if (select_rc == 1) {
		return 1;
	}
	goto fail;
#else  /* USE_SELECT */
	fds[0].fd = client_sock;
	fds[0].events = POLLIN;
	fds[0].revents = 0;

	poll_rc = poll(fds, 1, 0);
	if (poll_rc < 0) {
		alog("%s: poll returned < 0, closing connection: %s",
		        __FILE__, strerror(errno));
		goto fail;  /* also returns 0, which is correct */
	} else if (poll_rc > 1) {
		alog("%s: poll returned > 1, treating like 1",
		        __FILE__);
		return 1;  /* should never happen */
	} else if (poll_rc == 0) {
		return 0;  /* nothing to read */
	} else {
		return 1;  /* something to read */
	}
#endif  /* USE_SELECT */
 fail:
	if (client_sock >= 0) {
		(void) close(client_sock);
		client_sock = -1;
	}
	return 0;
}

void duk_trans_socket_read_flush_cb(void *udata) {
	(void) udata;  /* not needed by the example */

#if defined(DEBUG_PRINTS)
	alog("%s: udata=%p", __func__, (void *) udata);
#endif

	/* Read flush: Duktape may not be making any more read calls at this
	 * time.  If the transport maintains a receive window, it can use a
	 * read flush as a signal to update the window status to the remote
	 * peer.  A read flush is guaranteed to occur before Duktape stops
	 * reading for a while; it may occur in other situations as well so
	 * it's not a 100% reliable indication.
	 */

	/* This TCP transport requires no read flush handling so ignore.
	 * You can also pass a NULL to duk_debugger_attach() and not
	 * implement this callback at all.
	 */
}

void duk_trans_socket_write_flush_cb(void *udata) {
	(void) udata;  /* not needed by the example */

#if defined(DEBUG_PRINTS)
	alog("%s: udata=%p", __func__, (void *) udata);
#endif

	/* Write flush.  If the transport combines multiple writes
	 * before actually sending, a write flush is an indication
	 * to write out any pending bytes: Duktape may not be doing
	 * any more writes on this occasion.
	 */

	/* This TCP transport requires no write flush handling so ignore.
	 * You can also pass a NULL to duk_debugger_attach() and not
	 * implement this callback at all.
	 */
	return;
}
