#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>

#include <sys/stat.h>
#include <sys/types.h>
#ifndef _WIN32           /* Linux/OSX */
#include <sys/socket.h>
#include <sys/select.h>
#else                    /* Windows */
#include <winsock2.h>
#endif
#include <microhttpd.h>

#define DEFAULT_PORT 8888

#define FILENAME "picture.png"
#define MIMETYPE "image/png"

static int
handler ( void *cls, struct MHD_Connection *conn,
		const char *url, const char *method, const char *version,
		const char *upload_data, size_t *upload_data_size, void **con_cls ) {
	struct MHD_Response *response;
	int fd, ret;
	struct stat sbuf;

	if ( strcmp(method, "GET") != 0 ) {
		return MHD_NO;	
	}

	fd = open(FILENAME, O_RDONLY);
	if ( fd < 0 || fstat(fd, &sbuf) != 0 ) {
		/* error accessing file */
		if ( fd >= 0 ) { close(fd); }

		const char *msg = "<html><body>An internal server error has occured!\
			</body></html>";

		response = MHD_create_response_from_buffer(strlen(msg), (void*) msg,
				MHD_RESPMEM_PERSISTENT);

		if (response) {
			ret = MHD_queue_response(conn, MHD_HTTP_INTERNAL_SERVER_ERROR,
					response);
			MHD_destroy_response(response);
			return ret;
		} else {
			return MHD_NO;
		}
	}

	response = MHD_create_response_from_fd_at_offset64(sbuf.st_size, fd, 0);
	MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, MIMETYPE);
	ret = MHD_queue_response(conn, MHD_HTTP_OK, response);
	MHD_destroy_response(response);

	return ret;
}

int
main ( int argc, char *argv[] ) {
	struct MHD_Daemon *daemon;
	int port = DEFAULT_PORT;

	if ( argc > 1 ) {
		port = atoi(argv[1]);
	}

	daemon = MHD_start_daemon(
		MHD_USE_SELECT_INTERNALLY | MHD_USE_THREAD_PER_CONNECTION,
		port, NULL, NULL, &handler, NULL, MHD_OPTION_END );

	if (!daemon) { return EXIT_FAILURE; }

	(void) getc(stdin);

	MHD_stop_daemon(daemon);

	return EXIT_SUCCESS;
}