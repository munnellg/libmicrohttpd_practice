#include <time.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <microhttpd.h>

#define PORT   8888

int
answer_to_connection ( void *cls, struct MHD_Connection *connection,
		const char *url, const char *method, const char *version,
		const char *upload_data, size_t *upload_data_size, void **con_cls ) {
	static int aptr;
	struct MHD_Response *response;
	int ret;
	char page[128] = {0};
	
	/* Make sure this is a GET request */
	if ( strcmp(method, "GET") != 0 ) {	return MHD_NO; }

	if ( &aptr != *con_cls ) {
		/* Don't respond to the first request sent by client */
		*con_cls = &aptr;
		return MHD_YES;
	}

	/* reset now that we have processed this request */
	*con_cls = NULL;

	/* Get current date/time which we'll send to the user */
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	sprintf( page, "<html><body><h1>%d/%d/%d %d:%d:%d</h1></body></html>",
		tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec );

	/* Use the date/time string we just generated to create a response packet */
	response = MHD_create_response_from_buffer( strlen(page), (void*) page,
		MHD_RESPMEM_MUST_COPY );

	/* Queue the response to be sent to the client and release memory */
	ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);

	/* return the result of running this function */
	return ret;
}

int
main ( int argc, char *argv[] ) {
	struct MHD_Daemon *daemon;
	
	daemon = MHD_start_daemon(
		MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG, PORT, NULL, NULL,
		&answer_to_connection, NULL, MHD_OPTION_END );

	if (!daemon) { return 1; }

	getc(stdin);

	MHD_stop_daemon(daemon);

	return 0;
}
