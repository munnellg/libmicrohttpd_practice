#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <microhttpd.h>

#include <arpa/inet.h>

#define DEFAULT_PORT 8888

#define PAGE_INDEX     "<html><head><title>Home</title></head><body><a href='/another.html'>Another</a></body></html>"
#define PAGE_ANOTHER   "<html><head><title>Another</title></head><body>Hello There</body></html>"
#define PAGE_NOT_FOUND "<html><head><title>404</title></head><body>Page not found</body></html>"

int
log_client ( void *cls, const struct sockaddr *addr, socklen_t addrlen ) {
	char ip[16] = {0};	
	printf("%s\n", inet_ntop(addr->sa_family, addr->sa_data, ip, addrlen ));
	return MHD_YES;
}

int
log_header ( void *cls, enum MHD_ValueKind kind,
		const char *key, const char *value ) {
	printf("%s : %s\n", key, value);
	return MHD_YES;
}

int
explore ( void *cls, struct MHD_Connection *conn,
		const char *url, const char *method, const char *version,
		const char *upload_data, size_t *upload_data_size, void **con_cls ) {
	struct MHD_Response *response;
	char *page = NULL;
	int ret, status = MHD_HTTP_OK;

	printf("New %s request for %s using version %s\n", method, url, version);
	MHD_get_connection_values ( conn, MHD_HEADER_KIND, &log_header, NULL );

	if ( !strcmp(url, "/index.htm") || !strcmp(url, "/index.html") ) {
		page = PAGE_INDEX;
	} else if (!strcmp(url, "/another.htm") || !strcmp(url, "/another.html")) {
		page = PAGE_ANOTHER;
	} else {
		page = PAGE_NOT_FOUND;
		status = MHD_HTTP_NOT_FOUND;
	}

	response = MHD_create_response_from_buffer( strlen(page), (void*) page, 
			MHD_RESPMEM_MUST_COPY );


	ret = MHD_queue_response( conn, status, response );

	return ret;
}

int
main ( int argc, char *argv[] ) {
	int port = DEFAULT_PORT;
	struct MHD_Daemon *daemon;

	if (argc > 1) {
		port = atoi(argv[1]);
	}

	daemon = MHD_start_daemon(
		MHD_USE_SELECT_INTERNALLY, port,
		&log_client, NULL,
		&explore, NULL, MHD_OPTION_END );

	if (!daemon) { return EXIT_FAILURE; }

	(void) getc(stdin);

	MHD_stop_daemon(daemon);

	return EXIT_SUCCESS;
}