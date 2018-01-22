#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <sys/select.h>
#else
#include <winsock2.h>
#endif

#include <microhttpd.h>

#define MAX_NAME_SIZE    64
#define MAX_ANSWER_SIZE  256
#define POST_BUFFER_SIZE 256 // smaller than this causes error in microhttpd
#define DEFAULT_PORT     8888

#define GET  0
#define POST 1

#define PAGE_FORM  "<html><body>What's your name?<br/><form action='/namepost' method='post'><input name='name' type='text'/><input type='submit' value='Send'/></form></body></html>"

#define PAGE_GREET "<html><body><h1><center>Welcome %s!</center></h1></body></html>"

#define PAGE_ERROR "<html><body>This doesn't seem to be right.</body></html>"

struct connection_info_struct {
	int connection_type;
	char *answer_string;
	struct MHD_PostProcessor *post_processor;
};

static int g_quit = 0;

static void
terminate ( int sigterm ) {
	g_quit = 1;
}

static int
send_page ( struct MHD_Connection *connection, const char *page ) {
	struct MHD_Response *response;
	int ret;

	response = MHD_create_response_from_buffer( strlen(page), (void*) page,
		MHD_RESPMEM_PERSISTENT );

	ret = MHD_queue_response( connection, MHD_HTTP_OK, response );

	MHD_destroy_response(response);

	return ret;
}

static void
request_complete ( void *cls, struct MHD_Connection *connection, 
		void **con_cls, enum MHD_RequestTerminationCode toe ) {
	struct connection_info_struct *con_info = *con_cls;

	if ( con_info == NULL ) { return; }
	if ( con_info->connection_type == POST ) {
		MHD_destroy_post_processor( con_info->post_processor );
		if ( con_info->answer_string ) { free(con_info->answer_string); }
	}

	free(con_info);
	*con_cls = NULL;
}

static int
iterate_post ( void *coninfo_cls, enum MHD_ValueKind kind, const char *key, 
		const char *filename, const char *content_type,
		const char *transfer_encoding, const char *data,
		uint64_t off, size_t size ) {
	struct connection_info_struct *con_info = coninfo_cls;
	if ( strcmp( key, "name" ) == 0 ) {
		if ((size > 0) && (size <= MAX_NAME_SIZE)) {
			char *answer_string;
			answer_string = malloc(MAX_ANSWER_SIZE);
			if (!answer_string) { return MHD_NO; }

			snprintf( answer_string, MAX_ANSWER_SIZE, PAGE_GREET, data );
			con_info->answer_string = answer_string;
		} else {
			con_info->answer_string = NULL;
		}
		return MHD_NO;
	}

	return MHD_YES;
}

static int
handle_request ( void *cls, struct MHD_Connection *connection, 
		const char *url, const char *method, const char *version, 
		const char *upload_data, size_t *upload_data_size, void **con_cls ) {
	
	if ( *con_cls == NULL ) {

		struct connection_info_struct *con_info;

		con_info = malloc(sizeof(struct connection_info_struct));
		if (NULL == con_info) { return MHD_NO; }
		con_info->answer_string = NULL;

		if ( strcmp(method, "POST") == 0 ) {			
			con_info->post_processor = MHD_create_post_processor( connection,
				POST_BUFFER_SIZE, iterate_post, (void*) con_info );

			if ( con_info->post_processor == NULL ) {
				free(con_info);
				return MHD_NO;
			}

			con_info->connection_type = POST;
		} else {
			con_info->connection_type = GET;
		}
		*con_cls = (void*) con_info;
		return MHD_YES;
	}

	if ( strcmp( method, "GET" ) == 0 ) {
		return send_page( connection, PAGE_FORM );
	}

	if ( strcmp( method, "POST") == 0 ) {		
		struct connection_info_struct *con_info = *con_cls;

		if ( *upload_data_size != 0 ) {
			MHD_post_process( con_info->post_processor, upload_data,
				*upload_data_size );
			*upload_data_size = 0;
			return MHD_YES;
		} else if ( con_info->answer_string != NULL ) {
			return send_page( connection, con_info->answer_string );
		}
	}

	return send_page(connection, PAGE_ERROR);
}

int
main ( int argc, char *argv[] ) {
	struct MHD_Daemon *daemon;
	int port = DEFAULT_PORT;

	if ( argc > 1 ) {
		port = atoi(argv[1]);
	}

	if ( signal( SIGINT, terminate ) == SIG_ERR ) {
		fprintf(stderr, "Failed to connect signal hander\n");
		return EXIT_FAILURE;
	}

	fprintf(stdout, "Starting service on port %d... ", port);
	fflush(stdout);

	daemon = MHD_start_daemon( MHD_USE_SELECT_INTERNALLY, port,
		
		NULL, NULL,              /* authentication handler */
		
		&handle_request, NULL,   /* request handler        */

		MHD_OPTION_NOTIFY_COMPLETED,
		&request_complete, NULL, /* post request clean-up  */
		
		MHD_OPTION_END
	);

	if (!daemon) { 
		fprintf(stdout, "FAILED\n"); 		
		return EXIT_FAILURE;
	}

	fprintf(stdout, "OK\n");

	while (!g_quit) { sleep(1); }

	MHD_stop_daemon(daemon);

	return EXIT_SUCCESS;
}