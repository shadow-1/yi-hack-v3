#define LWS_DLL
#define LWS_INTERNAL
#include "libwebsockets.h"
#include "lejp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <math.h>

/**
 * Constants used in the plugin.
 */
#define NOTIFICATION_MAX_LENGTH 200
#define BUFFER_SIZE 10
#define CHUNK_SIZE 200
#define NUM_RETRY 3
#define SUCCESS 0
#define FAILURE 1

/**
 * Command to run to get current ISP details.
 */
char* args_get_info[] =
{
	"wget",
	"-O-",
	"-q",
	"-T",
	"8",
	"https://ipinfo.io",
	NULL
};

/**
 * Command to run to get current proxy details.
 */
char* args_get_proxy_info[] =
{
	"proxychains4",
	"wget",
	"-O-",
	"-q",
	"-T",
	"8",
	"https://ipinfo.io",
	NULL
};

/**
 * Array of JSON keys to decode.
 */
static const char * const proxy_json[] =
{
	"ip",
	"hostname",
	"city",
	"region",
	"country",
	"loc",
	"org",
	"postal"
};

/**
 * Enumeration of JSON keys to decode.
 */
enum proxy_enum
{
	IP,
	HOSTNAME,
	CITY,
	REGION,
	COUNTRY,
	LOCATION,
	NETWORK,
	POSTAL
};

/**
 * Enumeration of possible actions to be performed in the
 * libwebsockets write callback.
 */
enum lws_write_action
{
	SEND_INFO,
	SEND_PROXY_INFO,
	SEND_LOG,
	SEND_NOTIFICATION,
	SEND_PROGRESS
};

/**
 * Enumeration of Notification types.
 */
enum notification_type
{
	ERROR,
	WARNING,
	INFORMATION
};

/**
 * Enumeration of types of logs.
 */
enum log_type
{
	COMMAND,
	STDOUT,
	STDERR,
	RETURN
};

/**
 * Enumeration of possible types of tests that can be performed.
 */
enum test_type
{
	TEST_CONFIG,
	TEST_LIST,
	DOWNLOAD_LIST
};

/**
 * Enumeration of possible states when testing proxy server configuration.
 */
enum test_proxy_state
{
	IDLE,
	INIT,
	GET_INFO,
	GET_PROXY_INFO,
	CLOSE
};

/**
 * Data structure that contains all data for a Notification to be sent to the
 * web browser via Websockets.
 */
struct notification
{
	enum notification_type nt;
	int code;
	unsigned char message[NOTIFICATION_MAX_LENGTH];
};

/**
 * Data structure that contains all data for logs to be sent to the
 * web browser via Websockets.
 */
struct log
{
	enum log_type lt;
	int code;
	unsigned char message[NOTIFICATION_MAX_LENGTH];
};

/**
 * Data structure that contains all variables that are needed when sending
 * information about the test program through WebSocket connection.
 */
struct test_info
{
	short return_value;
	short iteration;
	unsigned char ip[16];
	unsigned char hostname[64];
	unsigned char city[32];
	unsigned char region[32];
	unsigned char country[4];
	unsigned char location[32];
	unsigned char network[128];
	unsigned char postal[32];
};

/**
 * Data structure that contains all variables that are stored per plugin instance.
 */
struct per_vhost_data__yi_hack_v3_test_proxy
{
	struct lws_context *context;
	struct lws_vhost *vhost;
	const struct lws_protocols *protocol;
};

/**
 * Data structure that contains all variables that are stored per session.
 */
struct per_session_data__yi_hack_v3_test_proxy
{
	struct lws *wsi;

	enum lws_write_action next_write_action[BUFFER_SIZE];
	struct log next_log[BUFFER_SIZE];
	struct notification next_notification[BUFFER_SIZE];
	int nwa_back, nwa_front, nwa_cur;

	enum test_type type;
	enum test_proxy_state state;
	struct test_info test_information;

	int outfd[2];
    int infd[2];
	int errfd[2];

	pid_t pid;

	struct lejp_ctx ctx;

	short iteration;

	unsigned char progress;
	unsigned char progress_num;
};

struct per_vhost_data__yi_hack_v3_test_proxy *vhd;

/**
 * JSON Parser callback function.
 * @param ctx: Structure that holds all information to parse JSON.
 * @param reason: Index of parsed JSON key-value pair.
 * @return success or failure.
 */
static char test_proxy_cb(struct lejp_ctx *ctx, char reason)
{
	// Session variables passed through to callback
	struct per_session_data__yi_hack_v3_test_proxy *pss =
			(struct per_session_data__yi_hack_v3_test_proxy *)ctx->user;

	/* we only match on the prepared path strings */
	if (!(reason & LEJP_FLAG_CB_IS_VALUE) || !ctx->path_match)
		return 0;

	// Set appropriate variables which have been parsed.
	switch (ctx->path_match - 1)
	{
		case IP:
			strcpy(pss->test_information.ip, ctx->buf);
			return 0;
		case HOSTNAME:
			strcpy(pss->test_information.hostname, ctx->buf);
			return 0;
		case CITY:
			strcpy(pss->test_information.city, ctx->buf);
			return 0;
		case REGION:
			strcpy(pss->test_information.region, ctx->buf);
			return 0;
		case COUNTRY:
			strcpy(pss->test_information.country, ctx->buf);
			return 0;
		case LOCATION:
			strcpy(pss->test_information.location, ctx->buf);
			return 0;
		case NETWORK:
			strcpy(pss->test_information.network, ctx->buf);
			return 0;
		case POSTAL:
			strcpy(pss->test_information.postal, ctx->buf);
			return 0;
	}
}

/**
 * Initialisation code that gets executed once per Websocket session.
 * @param pss: Structure that holds variables that persist per Websocket session.
 */
void session_init(struct per_session_data__yi_hack_v3_test_proxy *pss)
{
	pss->nwa_back = -1;
	pss->nwa_front = 0;
	pss->nwa_cur = 0;
	
	pss->state = IDLE;
	pss->outfd[0] = 0;
	pss->outfd[1] = 0;
	pss->infd[0] = 0;
	pss->infd[1] = 0;
	pss->errfd[0] = 0;
	pss->errfd[1] = 0;
	pss->pid = 0;
}

/**
 * Determine which callback to trigger based on what needs to be done next.
 * @param vhd: Structure that holds variables that persist across all sessions.
 * @param pss: Structure that holds variables that persist per Websocket session.
 */
void determine_next_action(struct per_vhost_data__yi_hack_v3_test_proxy *vhd
		, struct per_session_data__yi_hack_v3_test_proxy *pss)
{
	// If there is a buffer of items to write to WebSocket,
	// write to the WebSocket before calling the test callback again
	if (pss->nwa_cur > 0)
	{
		lws_callback_on_writable(pss->wsi);
	}
	else if (pss->state != IDLE)
	{
		sleep(1);
		lws_callback_all_protocol_vhost(vhd->vhost, vhd->protocol, LWS_CALLBACK_USER);
	}
}

/**
 * Fork a process which executes the test command
 * @param infd: Stdin pipe.
 * @param outfd: Stdout pipe.
 * @param errfd: Stderr pipe.
 * @param pid: PID for the forked process.
 * @param argv: Command to be executed.
 * @param status: Return value from process.
 * @param pss: Structure that holds variables that persist per Websocket session.
 * @return Return value from waitpid().
 */
pid_t execute_process(int infd[], int outfd[], int errfd[], pid_t *pid, char *argv[]
		, int *status, struct per_session_data__yi_hack_v3_test_proxy *pss)
{
	pid_t wpid;
	char buffer[NOTIFICATION_MAX_LENGTH];
	int count;
	int m, n;

	// If we have not forked a process yet, fork a process
	if (*pid == 0)
	{
		pipe(infd);
		pipe(outfd);
		pipe(errfd);

		// Fork process
		*pid = fork();

		// Child process
		if(!*pid)
		{
			// Duplicate stdout, stdin, stderr pipes
			dup2(infd[0], STDIN_FILENO);
			dup2(outfd[1], STDOUT_FILENO);
			dup2(errfd[1], STDERR_FILENO);
 
			// Close pipes not required by the child
			close(infd[1]);
			close(outfd[0]);
			close(errfd[0]);

			// Execute test program in the current process
			execvp(argv[0], argv);
		}
		// Parent process
		else
		{
			int flags;
			int i;

			// Setup flags for stdout, stdin, stderr pipes
			// so that reading does not block
			flags = fcntl(infd[1], F_GETFL, 0);
			if(fcntl(infd[1], F_SETFL, flags | O_NONBLOCK));
			flags = fcntl(outfd[0], F_GETFL, 0);
			if(fcntl(outfd[0], F_SETFL, flags | O_NONBLOCK));
			flags = fcntl(errfd[0], F_GETFL, 0);
			if(fcntl(errfd[0], F_SETFL, flags | O_NONBLOCK));
 
			// Close pipes not required by the parent
			close(infd[0]);
			close(outfd[1]);
			close(errfd[1]);

			// Send log with command that has been executed through websocket
			pss->nwa_back++;
			pss->next_log[pss->nwa_back%BUFFER_SIZE].lt = COMMAND;
			pss->next_log[pss->nwa_back%BUFFER_SIZE].message[0] = '\0';

			i = 0;
			do
			{
				strcat(pss->next_log[pss->nwa_back%BUFFER_SIZE].message, argv[i]);
				strcat(pss->next_log[pss->nwa_back%BUFFER_SIZE].message, " ");
				i++;
			} while (argv[i] != NULL);

			strcat(pss->next_log[pss->nwa_back%BUFFER_SIZE].message, "\n\n");
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_LOG;
			pss->nwa_cur++;
		}
	}

	// Read stderr pipe
	do
	{
		count = read(errfd[0], buffer, sizeof(buffer)-1);

		// Send log with current stderr contents through websocket
		if (count >= 0)
		{
			buffer[count] = '\0';

			pss->nwa_back++;
			pss->next_log[pss->nwa_back%BUFFER_SIZE].lt = STDERR;
			strcpy(pss->next_log[pss->nwa_back%BUFFER_SIZE].message, buffer);
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_LOG;
			pss->nwa_cur++;
		}
	} while (count > 0);

	// Read stdout pipe
	do
	{
		count = read(outfd[0], buffer, sizeof(buffer)-1);

		// Send log with current stdout contents through websocket
		if (count >= 0)
		{
			buffer[count] = '\0';

			// Parse the JSON formatted return value
			m = (int)(signed char)lejp_parse(&pss->ctx, buffer, count);

			pss->nwa_back++;
			pss->next_log[pss->nwa_back%BUFFER_SIZE].lt = STDOUT;
			strcpy(pss->next_log[pss->nwa_back%BUFFER_SIZE].message, buffer);
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_LOG;
			pss->nwa_cur++;
		}
	} while (count > 0);

	// Determine status of child process without blocking
	wpid = waitpid(*pid, status, WNOHANG);

	// Child process has finished...
	if (wpid != 0)
	{
		// If the child process ended gracefully...
		if (WIFEXITED(*status))
		{
			// Send log with return value through websocket
			pss->nwa_back++;
			pss->next_log[pss->nwa_back%BUFFER_SIZE].lt = RETURN;
			pss->next_log[pss->nwa_back%BUFFER_SIZE].code = WEXITSTATUS(*status);
			sprintf(pss->next_log[pss->nwa_back%BUFFER_SIZE].message,
					"\n\nReturn Code: %d\n\n", WEXITSTATUS(*status));
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_LOG;
			pss->nwa_cur++;
		}
		// If the child process ended abruptly...
		else if (WIFSIGNALED(*status))
		{
			pss->nwa_back++;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
			sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
					"ERROR (%d): Child process %d ended abruptly. Status Code: %d\n"
					, *pid, WTERMSIG(*status));
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
			pss->nwa_cur++;
			lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
			return -1;
		}
		return wpid;
	}
}

/**
 * Determine steps that need to be performed to test proxy configuration
 * based on the current state.
 * @param pss: Structure that holds variables that persist per Websocket session.
 */
void test_config(struct per_session_data__yi_hack_v3_test_proxy *pss)
{
	pid_t rv;
	int status;

	switch (pss->state)
	{
		case INIT:
			// If an instance of the test is not currently running,
			// start the testing process
			if (pss->pid == 0)
			{
				pss->iteration = 1;
				pss->progress = 1;
				pss->progress_num = 8;
				pss->state = GET_INFO;

				pss->nwa_back++;
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_PROGRESS;
				pss->nwa_cur++;
			}
			// Can only run the test one instance at a time per session
			else
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message
						, "ERROR: Already started testing proxy."
								"Can not run more than one instance at a time.\n"
						, errno);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				return;
			}

			break;

		case GET_INFO:
			lejp_construct(&pss->ctx, test_proxy_cb, pss, proxy_json,
					ARRAY_SIZE(proxy_json));
			rv = execute_process(pss->infd, pss->outfd, pss->errfd, &pss->pid
					, args_get_info, &status, pss);

			if (rv < 0)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"ERROR (%d): Could not start external process.\n"
						, errno);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				return;
			}
			else if (rv == 0)
			{
				// Process has not finished (callback needs to be called again)
			}
			else
			{
				// Process finished, cleanup and move onto next step
				lejp_destruct(&pss->ctx);
				pss->outfd[0] = 0;
				pss->outfd[1] = 0;
				pss->infd[0] = 0;
				pss->infd[1] = 0;
				pss->errfd[0] = 0;
				pss->errfd[1] = 0;
				pss->pid = 0;

				// Send current progress
				pss->progress = pss->iteration + 1;
				pss->nwa_back++;
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_PROGRESS;
				pss->nwa_cur++;

				// Get return value, interation and send through WebSocket connection
				pss->test_information.return_value = WEXITSTATUS(status);
				pss->test_information.iteration = pss->iteration;

				pss->nwa_back++;
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_INFO;
				pss->nwa_cur++;

				// If process completed successfully, move onto the next step
				if (status == 0)
				{
					// Half way through the test
					pss->progress = 4;
					pss->nwa_back++;
					pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_PROGRESS;
					pss->nwa_cur++;

					pss->iteration = 1;
					pss->state = GET_PROXY_INFO;
				}
				else
				{
					// If the process failed, retry
					if (pss->iteration < NUM_RETRY)
					{
						pss->iteration++;
					}
					// Otherwise send error message and abort
					else
					{
						pss->nwa_back++;
						pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
						sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
								"Could not connect to ipinfo.io, "
								"ensure that you are connectected to the Internet.\n");
						pss->next_write_action[pss->nwa_back%BUFFER_SIZE] =
								SEND_NOTIFICATION;
						pss->nwa_cur++;
						lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE]
								.message);
						pss->state = CLOSE;
					}
				}
			}

			break;

		case GET_PROXY_INFO:
			lejp_construct(&pss->ctx, test_proxy_cb, pss, proxy_json
					, ARRAY_SIZE(proxy_json));
			rv = execute_process(pss->infd, pss->outfd, pss->errfd, &pss->pid
					, args_get_proxy_info, &status, pss);

			if (rv < 0)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"ERROR (%d): Could not start external process.\n"
						, errno);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				return;
			}
			else if (rv == 0)
			{
				// Process has not finished (callback needs to be called again)
			}
			else
			{
				// Process finished, cleanup and move onto next step
				lejp_destruct(&pss->ctx);
				pss->outfd[0] = 0;
				pss->outfd[1] = 0;
				pss->infd[0] = 0;
				pss->infd[1] = 0;
				pss->errfd[0] = 0;
				pss->errfd[1] = 0;
				pss->pid = 0;

				// Send current progress
				pss->progress = pss->iteration + 4;
				pss->nwa_back++;
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_PROGRESS;
				pss->nwa_cur++;

				// Get return value, interation and send through WebSocket connection
				pss->test_information.return_value = WEXITSTATUS(status);
				pss->test_information.iteration = pss->iteration;

				pss->nwa_back++;
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_PROXY_INFO;
				pss->nwa_cur++;

				// If process completed successfully, move onto the next step
				if (status == 0)
				{
					// About 90% through the test, only cleanup to go
					pss->progress = 7;
					pss->nwa_back++;
					pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_PROGRESS;
					pss->nwa_cur++;

					// If the detected country is not Mainland China,
					// fail the test and send a warning through the WebSocket connection
					if (strcmp(pss->test_information.country, "CN"))
					{
						pss->test_information.return_value = 1;

						pss->nwa_back++;
						pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = WARNING;
						sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
							"ProxyChains-ng configuration appears to contain at least "
							"one proxy server that is located outside Mainland China. "
							"Ensure all configured proxy servers are located within "
							"Mainland China.");
						pss->next_write_action[pss->nwa_back%BUFFER_SIZE] =
								SEND_NOTIFICATION;
						pss->nwa_cur++;
					}

					pss->iteration = 1;
					pss->state = CLOSE;
				}
				// Otherwise send error message and abort
				else
				{
					// If the process failed, retry
					if (pss->iteration < NUM_RETRY)
					{
						pss->iteration++;
					}
					// Otherwise send error message and abort
					else
					{
						pss->nwa_back++;
						pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
						sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
								"Could not connect to ipinfo.io through ProxyChains-ng, "
								"ensure that the configured proxy servers are operational.\n");
						pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
						pss->nwa_cur++;
						lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
						pss->state = CLOSE;
					}
				}
			}

			break;

		case CLOSE:
			pss->state = IDLE;

			// Finished the test
			pss->progress = 8;
			pss->nwa_back++;
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_PROGRESS;
			pss->nwa_cur++;

			pss->nwa_back++;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = INFORMATION;
			sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
					"ProxyChains-ng test completed.");
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
			pss->nwa_cur++;

			break;

		default:
			break;
	}
}

/**
 * Actions that are taken when reading from the Websocket.
 * @param vhd: Structure that holds variables that persist across all sessions.
 * @param pss: Structure that holds variables that persist per Websocket session.
 * @param in: Data that has been received from the Websocket session.
 * @param buf: String buffer.
 * @return success or failure.
 */
int session_read(struct per_vhost_data__yi_hack_v3_test_proxy *vhd
		, struct per_session_data__yi_hack_v3_test_proxy *pss, char *in, char *buf)
{
	char *token;

	// Split the incoming data by the newline
	token = strtok((char *)in, "\n\0");

	// If nothing is found after the split, return error
	if (token == NULL)
	{
		return -1;
	}

	// Received valid request
	if (strcmp(token, "TEST_CONFIG") == 0 || strcmp(token, "TEST_LIST") == 0 ||
			strcmp(token, "DOWNLOAD_LIST") == 0)
	{
		// Can only run one test at a time per session
		if (pss->state != IDLE)
		{
			pss->nwa_back++;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
			sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message
					, "ERROR: Already started testing proxy. "
					"Can not run more than one instance at a time.\n"
					, errno);
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
			pss->nwa_cur++;
			lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
			return -1;
		}

		// Set the appropriate test to be performed
		if (strcmp(token, "TEST_CONFIG") == 0)
		{
			pss->type = TEST_CONFIG;
		}
		else if (strcmp(token, "TEST_LIST") == 0)
		{
			pss->type = TEST_LIST;
		}
		else if (strcmp(token, "DOWNLOAD_LIST") == 0)
		{
			pss->type = DOWNLOAD_LIST;
		}

		pss->state = INIT;
		lws_callback_all_protocol_vhost(vhd->vhost, vhd->protocol, LWS_CALLBACK_USER);
	}
	return 0;
}

/**
 * Actions that are taken when writing to the Websocket.
 * @param vhd: Structure that holds variables that persist across all sessions.
 * @param pss: Structure that holds variables that persist per Websocket session.
 * @param buf: String buffer.
 * @return success or failure.
 */
int session_write(struct per_vhost_data__yi_hack_v3_test_proxy *vhd,
		struct per_session_data__yi_hack_v3_test_proxy *pss, char *buf)
{
	int n, m;
	float f;
	char temp_string[2*CHUNK_SIZE];
	
	// Send the next message in the FIFO action buffer
	switch (pss->next_write_action[pss->nwa_front%BUFFER_SIZE])
	{
		case SEND_INFO:
			// Prepare to send data in JSON format
			n = sprintf((char *)buf, "{\n\"message\":\"SEND_INFO\",\n"
					"\"return_value\":\"%d\",\n\"iteration\":\"%d\",\n\"ip\":\"%s\",\n"
					"\"hostname\":\"%s\",\n\"city\":\"%s\",\n\"region\":\"%s\",\n"
					"\"country\":\"%s\",\n\"loc\":\"%s\",\n\"org\":\"%s\",\n"
					"\"postal\":\"%s\"\n}"
					, pss->test_information.return_value, pss->test_information.iteration
					, pss->test_information.ip, pss->test_information.hostname, pss->test_information.city
					, pss->test_information.region, pss->test_information.country
					, pss->test_information.location, pss->test_information.network
					, pss->test_information.postal);

			// Send the data
			m = lws_write(pss->wsi, buf, n, LWS_WRITE_TEXT);
			if (m < n)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"ERROR (%d): Error writing to yi-hack-v3_test_proxy Websocket.\n"
						, errno);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				return -1;
			}

			// Remove action from the FIFO buffer
			pss->nwa_front++;
			pss->nwa_cur--;

			strcpy(pss->test_information.ip, "");
			strcpy(pss->test_information.hostname, "");
			strcpy(pss->test_information.city, "");
			strcpy(pss->test_information.region, "");
			strcpy(pss->test_information.country, "");
			strcpy(pss->test_information.location, "");
			strcpy(pss->test_information.network, "");
			strcpy(pss->test_information.postal, "");
			break;

		case SEND_PROXY_INFO:
			// Prepare to send data in JSON format
			n = sprintf((char *)buf, "{\n\"message\":\"SEND_PROXY_INFO\",\n"
					"\"return_value\":\"%d\",\n\"iteration\":\"%d\",\n\"ip\":\"%s\",\n"
					"\"hostname\":\"%s\",\n\"city\":\"%s\",\n\"region\":\"%s\",\n"
					"\"country\":\"%s\",\n\"loc\":\"%s\",\n\"org\":\"%s\",\n"
					"\"postal\":\"%s\"\n}"
					, pss->test_information.return_value, pss->test_information.iteration
					, pss->test_information.ip, pss->test_information.hostname, pss->test_information.city
					, pss->test_information.region, pss->test_information.country
					, pss->test_information.location, pss->test_information.network
					, pss->test_information.postal);

			// Send the data
			m = lws_write(pss->wsi, buf, n, LWS_WRITE_TEXT);
			if (m < n)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"ERROR (%d): Error writing to yi-hack-v3_info Websocket.\n"
						, errno);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				return -1;
			}

			// Remove action from the FIFO buffer
			pss->nwa_front++;
			pss->nwa_cur--;

			strcpy(pss->test_information.ip, "");
			strcpy(pss->test_information.hostname, "");
			strcpy(pss->test_information.city, "");
			strcpy(pss->test_information.region, "");
			strcpy(pss->test_information.country, "");
			strcpy(pss->test_information.location, "");
			strcpy(pss->test_information.network, "");
			strcpy(pss->test_information.postal, "");
			break;
		case SEND_LOG:
			// Prepare to send data in JSON format
			lws_json_purify(temp_string,
					pss->next_log[pss->nwa_front%BUFFER_SIZE].message, 2*CHUNK_SIZE);

			n = sprintf((char *)buf, "{\n\"message\":\"SEND_LOG\",\n\""
					"l_type\":\"%d\",\n\"l_code\":\"%d\",\n\"l_message\":\"%s\"\n}"
					, pss->next_log[pss->nwa_front%BUFFER_SIZE].lt
					, pss->next_log[pss->nwa_front%BUFFER_SIZE].code
					, temp_string);

			// Send the data
			m = lws_write(pss->wsi, buf, n, LWS_WRITE_TEXT);
			if (m < n)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"ERROR (%d): Error writing to yi-hack-v3_info Websocket.\n"
						, errno);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				return -1;
			}

			// Remove action from the FIFO buffer
			pss->nwa_front++;
			pss->nwa_cur--;
			break;

		case SEND_NOTIFICATION:
			// Prepare to send data in JSON format
			lws_json_purify(temp_string,
					pss->next_notification[pss->nwa_front%BUFFER_SIZE].message
					, 2*CHUNK_SIZE);

			n = sprintf((char *)buf, "{\n\"message\":\"SEND_NOTIFICATION\",\n\""
					"n_type\":\"%d\",\n\"n_number\":\"%d\",\n\"n_message\":\"%s\"\n}"
					, pss->next_notification[pss->nwa_front%BUFFER_SIZE].nt
					, pss->next_notification[pss->nwa_front%BUFFER_SIZE].code
					, temp_string);

			// Send the data
			m = lws_write(pss->wsi, buf, n, LWS_WRITE_TEXT);
			if (m < n)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"ERROR (%d): Error writing to yi-hack-v3_test_proxy Websocket.\n"
						, errno);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				return -1;
			}

			// Remove action from the FIFO buffer
			pss->nwa_front++;
			pss->nwa_cur--;
			break;

		case SEND_PROGRESS:
			f = ceil((float)pss->progress/pss->progress_num * 100);

			// Prepare to send data in JSON format
			n = sprintf((char *)buf, "{\n\"message\":\"SEND_PROGRESS\",\n"
					"\"percentage\":\"%d\"\n}", (int)f);

			// Send the data
			m = lws_write(pss->wsi, buf, n, LWS_WRITE_TEXT);
			if (m < n)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"ERROR (%d): Error writing to yi-hack-v3_test_proxy Websocket.\n"
						, errno);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				return -1;
			}

			// Remove action from the FIFO buffer
			pss->nwa_front++;
			pss->nwa_cur--;
		
		default:
			break;
	}
}

/**
 * yi-hack-v3_test_proxy libwebsockets plugin callback function.
 * @param reason: The reason why the callback has been executed.
 * @param user: Per session data.
 * @param in: String that contains data sent via Websocket connection.
 * @param len: Length of data sent via Websocket connection.
 * @return success or failure.
 */
static int
callback_yi_hack_v3_test_proxy(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	struct per_session_data__yi_hack_v3_test_proxy *pss =
				(struct per_session_data__yi_hack_v3_test_proxy *)user;
	unsigned char buf[LWS_PRE + 512];
	unsigned char *p = &buf[LWS_PRE];
	
	vhd = (struct per_vhost_data__yi_hack_v3_test_proxy *)lws_protocol_vh_priv_get
				(lws_get_vhost(wsi), lws_get_protocol(wsi));

	switch (reason)
	{
		case LWS_CALLBACK_PROTOCOL_INIT:
			vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi), lws_get_protocol(wsi)
					, sizeof(struct per_vhost_data__yi_hack_v3_test_proxy));
			vhd->context = lws_get_context(wsi);
			vhd->protocol = lws_get_protocol(wsi);
			vhd->vhost = lws_get_vhost(wsi);

			break;

		case LWS_CALLBACK_PROTOCOL_DESTROY:
			if (!vhd)
				break;

			break;

		case LWS_CALLBACK_ESTABLISHED:
			pss->wsi = wsi;
			session_init(pss);

			break;

		case LWS_CALLBACK_SERVER_WRITEABLE:
			if (pss->nwa_cur > 0)
			{
				session_write(vhd, pss, p);
				determine_next_action(vhd, pss);
			}

			break;

		case LWS_CALLBACK_RECEIVE:
			session_read(vhd, pss, (char *)in, p);

			break;

		case LWS_CALLBACK_USER:
			switch (pss->type)
			{
				case TEST_CONFIG:
					test_config(pss);
					break;
				case TEST_LIST:
					break;
				case DOWNLOAD_LIST:
					break;
			}
			determine_next_action(vhd, pss);

			break;

		default:
			break;
		}

		return 0;
}

static const struct lws_protocols protocols[] = {
	{
		"yi-hack-v3_test_proxy",
		callback_yi_hack_v3_test_proxy,
		sizeof(struct per_session_data__yi_hack_v3_test_proxy),
		1024, /* rx buf size must be >= permessage-deflate rx size */
	},
};

LWS_VISIBLE int
init_protocol_yi_hack_v3_test_proxy(struct lws_context *context,
			     struct lws_plugin_capability *c)
{
	if (c->api_magic != LWS_PLUGIN_API_MAGIC) {
		lwsl_err("Plugin API %d, library API %d", LWS_PLUGIN_API_MAGIC,
			 c->api_magic);
		return 1;
	}

	c->protocols = protocols;
	c->count_protocols = ARRAY_SIZE(protocols);

	return 0;
}

LWS_VISIBLE int
destroy_protocol_yi_hack_v3_test_proxy(struct lws_context *context)
{
	return 0;
}
