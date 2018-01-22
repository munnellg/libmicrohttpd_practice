#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include <sys/types.h>
#ifndef _WIN32
#include <sys/select.h>
#include <sys/socket.h>
#else
#include <winsock2.h>
#endif

#include <microhttpd.h>

#define MAX_CLIENTS  2
#define DEFAULT_PORT 8888

#define POST_BUFFER_SIZE 512

#define PAGE_BUSY  "<html><body>The server is busy. Please try again later</body></html>"

#define PAGE_FORM  "<html><body>Upload a file, please!<br/>There are %u clients uploading at the moment. <br/><form action='/filepost' method='post' enctype='multipart/form-data'><input name='file' type='file'/><input type='submit' value='Send'/></form></body></html>"

#define PAGE_DONE  "<html><body>The upload has been completed</body></html>"

#define PAGE_ERROR "<html><body>An internal server error has occurred.</body></html>"

#define PAGE_EXIST "<html><body>This file already exists</body></html>"

enum ConnectionType { GET, POST };

struct connection_info_struct {
	FILE *fp;
	const char *answer_string;
	struct MHD_PostProcessor *post_processor;	
	enum ConnectionType connection_type;
	int answer_code;
};

static int g_quit = 0;
static unsigned int g_num_uploading_clients = 0;

static void
terminate ( int sig ) {
	g_quit = 1;
}

static int
send_page ( struct MHD_Connection *connection,
		const char *page, int status_code ) {
	int ret;
	struct MHD_Response *response;

	response = MHD_create_response_from_buffer( strlen(page), (void*) page, 
		MHD_RESPMEM_MUST_COPY );

	if (!response) { return MHD_NO; }

	ret = MHD_queue_response( connection, status_code, response );
	MHD_destroy_response(response);

	return ret;
}

static int
iterate_post ( void *coninfo_cls, enum MHD_ValueKind kind,
		const char *key,
		const char *filename, const char *content_type,
		const char *transfer_encoding, const char *data, 
		uint64_t off, size_t size ) {

	FILE *fp;
	struct connection_info_struct *con_info = coninfo_cls;
	con_info->answer_string = PAGE_ERROR;
	con_info->answer_code = MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	if ( strcmp(key, "file") != 0 ) { return MHD_NO; }
	
	if ( !con_info->fp ) {
		if ( (fp = fopen(filename, "rb")) != NULL ) {
			fclose(fp);
			con_info->answer_string = PAGE_EXIST;
			con_info->answer_code = MHD_HTTP_FORBIDDEN;
			return MHD_NO;
		}

		con_info->fp = fopen(filename, "ab");
		if (!con_info->fp) { return MHD_NO; }		
	}

	if ( size > 0 ) {
		if ( !fwrite(data, size, sizeof(char), con_info->fp) ) {
			return MHD_NO;
		}
	}

	con_info->answer_string = PAGE_DONE;
	con_info->answer_code = MHD_HTTP_OK;

	return MHD_YES;
}

static void
request_complete ( void *cls, struct MHD_Connection *connection,
		void **con_cls,
		enum MHD_RequestTerminationCode toe ) {
	
	struct connection_info_struct *con_info = *con_cls;

	if ( NULL == con_info ) { return; }

	if ( con_info->connection_type == POST ) {
		if ( NULL != con_info->post_processor ) {
			MHD_destroy_post_processor(con_info->post_processor);
			g_num_uploading_clients--;
		}
		if ( con_info->fp ) { fclose(con_info->fp); }
	}

	free(con_info);
	*con_cls = NULL;
}

static int
handle_request ( void *cls, struct MHD_Connection *connection, 
		const char *url, const char *method, const char *version, 
		const char *upload_data, size_t *upload_data_size, void **con_cls ) {

	if ( NULL == *con_cls ) {
		struct connection_info_struct *con_info;

		if (g_num_uploading_clients >= MAX_CLIENTS) {
			return send_page( connection, PAGE_BUSY,
				MHD_HTTP_SERVICE_UNAVAILABLE );
		}

		con_info = malloc(sizeof(struct connection_info_struct));
		if ( NULL==con_info ) { return MHD_NO; }
		con_info->fp = 0;

		if ( strcmp(method, "POST") == 0 ) {
			con_info->post_processor = MHD_create_post_processor( connection,
				POST_BUFFER_SIZE, iterate_post, (void*) con_info );

			if ( con_info->post_processor == NULL ) {
				free(con_info);
				return MHD_NO;
			}

			g_num_uploading_clients++;

			con_info->connection_type = POST;
			con_info->answer_code = MHD_HTTP_OK;
			con_info->answer_string = PAGE_DONE;
		} else {
			con_info->connection_type = GET;
		}

		*con_cls = (void*) con_info;
		return MHD_YES;
	}

	if ( strcmp( method, "GET" ) == 0 ) {		
		char buffer[1024];
		sprintf(buffer, PAGE_FORM, g_num_uploading_clients);
		return send_page(connection, buffer, MHD_HTTP_OK);
	}

	if ( strcmp( method, "POST" ) == 0 ) {
		struct connection_info_struct *con_info = *con_cls;

		if ( *upload_data_size != 0 ) {
			MHD_post_process( con_info->post_processor, upload_data,
				*upload_data_size );
			*upload_data_size = 0;
			return MHD_YES;
		} else {
			return send_page( connection, con_info->answer_string,
				con_info->answer_code );
		}
	}

	return send_page(connection, PAGE_ERROR, MHD_HTTP_BAD_REQUEST);
}

int
main ( int argc, char *argv[] ) {
	struct MHD_Daemon *daemon;
	int port = DEFAULT_PORT;

	/* allow user to specify alternative port for listening */
	if ( argc > 1 ) {
		port = atoi(argv[1]);
	}

	/* register listener for SIGINT so we can shut down gracefully */
	if ( signal(SIGINT, terminate) == SIG_ERR ) {
		fprintf(stderr, "Unable to register signal handler\n");
		return EXIT_FAILURE;
	}

	/* start the server */
	fprintf(stdout, "Initializing on port %d... ", port);
	fflush(stdout);
	daemon = MHD_start_daemon( MHD_USE_SELECT_INTERNALLY, port,
		NULL, NULL,
		&handle_request, NULL,
		MHD_OPTION_NOTIFY_COMPLETED,
		&request_complete, NULL,
		MHD_OPTION_END
	);

	/* check if initialization succeeded */
	if (!daemon) { fprintf(stdout, "FAILED\n"); return EXIT_FAILURE; }

	fprintf(stdout, "OK\n");

	/* loop while the daemon does its thing in another thread */
	while (!g_quit) {
		/* make this thread sleep in second intervals while we wait for the  */
		/* server to quit so we don't kill the processor with useless cycles */
		sleep(1);
	}

	/* shutdown the server daemon */
	fprintf(stdout, "Quitting\n");
	MHD_stop_daemon(daemon);

	return EXIT_SUCCESS;
}