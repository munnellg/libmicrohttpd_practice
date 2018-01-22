#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <sys/select.h>
#else
#include <winsock.h>
#endif
#include <microhttpd.h>

#define DEFAULT_PORT 8888
#define LIMIT        60

int
handle_request ( void *cls, struct MHD_Connection *conn,
		const char *url, const char *method, const char *version,
		const char *upload_data, size_t *upload_data_size, void **con_cls ) {
	struct MHD_Response *response;
	char buf[64] = {0};
	time_t now = time(NULL);
	double diff = difftime(now, *((time_t*) cls));
	int ret;

	if ( diff < LIMIT ) {		
		sprintf(buf, "%d", (int)(LIMIT - diff));
	} else {
		sprintf(buf, "DONE!");
	}

	response = MHD_create_response_from_buffer( 
		strlen(buf), (void *) buf, MHD_RESPMEM_MUST_COPY );
	MHD_add_response_header( response, "Refresh", "5; url=/");
	ret = MHD_queue_response( conn, MHD_HTTP_OK, response );

	return ret;
}

int
main ( int argc, char *argv[] ) {
	struct MHD_Daemon *daemon;
	int port = DEFAULT_PORT;
	time_t start = time(NULL);

	if (argc > 1) {
		port = atoi(argv[1]);
	}

	daemon = MHD_start_daemon( MHD_USE_SELECT_INTERNALLY, port,	NULL, NULL,
		&handle_request, &start, MHD_OPTION_END );

	if (!daemon) {
		return EXIT_FAILURE;
	}

	getchar();

	MHD_stop_daemon(daemon);

	return EXIT_SUCCESS;
}