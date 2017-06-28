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
#include <stdbool.h>

/**
 * Constants used in the plugin.
 */
#define PROXYCHAINSNG_CONFIG_FILE "/home/app/proxychains.conf"
#define TEMP_PROXYCHAINSNG_CONFIG_FILE "/tmp/temp_proxychains.conf"
#define TEMP_DOWNLOAD_FILE "/tmp/temp_proxy_list.txt"
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
 * Command to run to test temporary ProxyChains-ng configuration.
 */
char* args_get_proxy_info_temp[] =
{
	"proxychains4",
	"-f",
	TEMP_PROXYCHAINSNG_CONFIG_FILE,
	"wget",
	"-O-",
	"-q",
	"-T",
	"8",
	"https://ipinfo.io/geo",
	NULL
};

/**
 * Command to run to download proxy file.
 */
char* args_download_list[] =
{
	"wget",
	"-O",
	TEMP_DOWNLOAD_FILE,
	"https://freevpn.ninja/free-proxy/txt",
	NULL
};

/**
 * Base ProxyChains-ng config for temporary config file.
 */
char base_config[] =
	"random_chain\n"
	"chain_len = 1\n"
	"tcp_read_time_out 15000\n"
	"tcp_connect_time_out 8000\n"
	"[ProxyList]\n";

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
	SEND_PROGRESS,
	SEND_LIST
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
	DOWNLOAD_FILE,
	PARSE_FILE,
	PREP_TEMP_CONFIG_FILE,
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
 * Linked List of proxy servers in ProxyChains-ng format.
 */
struct proxy
{
	unsigned int idx;
	unsigned int num;
	char *line;
	struct proxy *next;
};

/**
 * Data structure that contains all variables that are needed when sending
 * information about the test program through WebSocket connection.
 */
struct test_info
{
	unsigned int index;
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
	struct lws_plat_file_ops *fops_plat;
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
	bool stopping;
	short iteration;

	int outfd[2];
    int infd[2];
	int errfd[2];
	pid_t pid;

	struct lejp_ctx ctx;

	unsigned int progress;
	unsigned int progress_num;

	struct proxy *head;
	struct proxy *current;

	long download_num;
	bool download_all;
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

	pss->head = NULL;
	pss->current = NULL;

	pss->stopping = false;

	pss->download_all = true;
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
 * Fork a process which executes the test command.
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
 * Cleanup variables and dynamically allocated memory assuming that the cleanup is
 * being called asynchronously.
 * @param pss: Structure that holds variables that persist per Websocket session.
 */
void cleanup(struct per_session_data__yi_hack_v3_test_proxy *pss)
{
	// Cleanup dynamically allocated proxy list
	while (pss->head != NULL)
	{
		pss->current = pss->head;
		pss->head = pss->head->next;

		if (pss->current->line != NULL)
		{
			free (pss->current->line);
			pss->current->line = NULL;
		}
		free(pss->current);
	}

	pss->head = NULL;
	pss->current = NULL;

	// If a child process is still running, terminate it and cleanup
	if (pss->pid != 0)
	{
		kill(pss->pid, SIGKILL);
		pss->outfd[0] = 0;
		pss->outfd[1] = 0;
		pss->infd[0] = 0;
		pss->infd[1] = 0;
		pss->errfd[0] = 0;
		pss->errfd[1] = 0;
		pss->pid = 0;
	}

	pss->stopping = false;
}

long rand_num(long min, long max)
{
    long r;
    const long range = 1 + max - min;
    const long buckets = RAND_MAX / range;
    const long limit = buckets * range;

    do
    {
        r = rand();
    } while (r >= limit);

    return min + (r / buckets);
}

void randomise_download_list(struct per_session_data__yi_hack_v3_test_proxy *pss, long total)
{
	struct proxy *new_head;
	struct proxy *new_current;
	struct proxy *link;
	long i, j, k;

	new_head = NULL;
	new_current = NULL;

	pss->current = pss->head;

	srand((unsigned int)time(NULL));

	for (i = 1; i <= pss->download_num; i++)
	{
		j = rand_num(1, total);

		for (k = 1; k < j; k++)
		{
			pss->current = pss->current->next;
		}

		// Dynamically allocate new node for Linked List
		link = (struct proxy*)malloc(sizeof(struct proxy));
		link->line = (char*)malloc(strlen(pss->current->line) + 1);
		strcpy(link->line, pss->current->line);
		link->idx = i;
		link->num = pss->current->num;
		link->next = NULL;

		// If there is no head node, this node will become the head
		if (new_head == NULL)
		{
			new_head = link;
			new_current = link;
		}
		// Otherwise join the end of the Linked List
		else
		{
			new_current->next = link;
			new_current = link;
		}

		pss->current = pss->head;
	}

	// Cleanup dynamically allocated proxy list
	while (pss->head != NULL)
	{
		pss->current = pss->head;
		pss->head = pss->head->next;

		if (pss->current->line != NULL)
		{
			free (pss->current->line);
			pss->current->line = NULL;
		}
		free(pss->current);
	}

	pss->head = new_head;
	pss->current = new_current;
}

/**
 * Execute steps that need to be performed to download and test each proxy server
 * that is flagged as being based in Mainland China from the free proxy server
 * list made available on freevpn.ninja based on the current state.
 * @param pss: Structure that holds variables that persist per Websocket session.
 */
void download_list(struct per_vhost_data__yi_hack_v3_test_proxy *vhd
		, struct per_session_data__yi_hack_v3_test_proxy *pss, char *buf)
{
	pid_t rv;
	int status;
	bool partial_read;
	int i;

	lws_fop_fd_t fop_fd;
	lws_filepos_t fop_len;
	lws_fop_flags_t flags;
	int file_ret;
	char *token;
	char *ret;

	struct proxy *link;

	char temp_string[512];
	char ip[16];
	char port[6];
	char proxy[7];
	char country[3];

	switch (pss->state)
	{
		case INIT:
			pss->iteration = 1;
			pss->progress = 1;

			if (pss->stopping)
			{
				pss->state = CLOSE;
			}
			else
			{
				pss->state = DOWNLOAD_FILE;
			}

			break;

		case DOWNLOAD_FILE:
			// If a request came in to stop the test,
			// kill the child process if it has started
			if (pss->stopping)
			{
				if (pss->pid != 0)
				{
					kill(pss->pid, SIGKILL);
				}
			}

			// Execute child process to download proxy file
			lejp_construct(&pss->ctx, test_proxy_cb, pss, proxy_json,
					ARRAY_SIZE(proxy_json));
			rv = execute_process(pss->infd, pss->outfd, pss->errfd, &pss->pid
					, args_download_list, &status, pss);

			if (rv < 0)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"ERROR (%d): Could not start external process.\n", errno);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				pss->state = CLOSE;
				return;
			}
			else if (rv == 0)
			{
				// Process has not finished (callback needs to be called again)
				break;
			}
			else
			{
				// Process finished, cleanup and move onto next step
				pss->outfd[0] = 0;
				pss->outfd[1] = 0;
				pss->infd[0] = 0;
				pss->infd[1] = 0;
				pss->errfd[0] = 0;
				pss->errfd[1] = 0;
				pss->pid = 0;

				// If process completed successfully, move onto the next step
				if (status == 0)
				{
					pss->state = PARSE_FILE;
				}
				else
				{
					if (!pss->stopping)
					{
						pss->nwa_back++;
						pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
						sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
								"Could not connect to freevpn.ninja, "
								"ensure that you are connectected to the Internet.\n");
						pss->next_write_action[pss->nwa_back%BUFFER_SIZE] =
								SEND_NOTIFICATION;
						pss->nwa_cur++;
						lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE]
								.message);
					}
					pss->state = CLOSE;
				}
			}

			break;

		case PARSE_FILE:
			partial_read = false;
			i = 0;

			// Open downloaded file and read proxy server lines
			flags = LWS_O_RDONLY;
			fop_fd = lws_vfs_file_open(vhd->fops_plat, TEMP_DOWNLOAD_FILE, &flags);

			if (!fop_fd)
			{
				lwsl_err("ERROR (%d): Failed to open file - %s.\n", errno,
						TEMP_DOWNLOAD_FILE);
				pss->state = CLOSE;
				return;
			}

			// Read contents of the downloaded file
			file_ret = lws_vfs_file_read(fop_fd, &fop_len, buf, 512);

			if (file_ret < 0)
			{
				lwsl_err("ERROR (%d): Failed to read file - %s.\n", errno,
						TEMP_DOWNLOAD_FILE);
				pss->state = CLOSE;
				return;
			}

			while (fop_len > 0)
			{
				buf[fop_len] = '\0';
				token = buf;

				ret = strchr(buf, '\n');

				// If a last line was partially read from the last chunk
				if (partial_read)
				{
					// Handle the case that we are at the last line and there are
					// no further new line characters to be read
					if (ret == NULL && fop_len < 512)
					{
						ret = strchr(buf, '\0');
					}

					if (ret != NULL)
					{
						// Join the last partial line read from the last chunk to the
						// end of the first line in the new chunk
						*ret = '\0';
						strcat(temp_string, token);

						// Read IP Address if available
						token = strtok(temp_string, ": ");
						if (token == NULL)
						{
							strcpy(ip, "");
						}
						else
						{
							strcpy(ip, token);
						}

						// Read Port if available
						token = strtok(NULL, ": ");
						if (token == NULL)
						{
							strcpy(port, "");
						}
						else
						{
							strcpy(port, token);
						}

						// Read Proxy type if available
						token = strtok(NULL, ": ");
						if (token == NULL)
						{
							strcpy(proxy, "");
						}
						else
						{
							strcpy(proxy, token);
						}

						// Read Country if available
						token = strtok(NULL, ": ");
						if (token == NULL)
						{
							strcpy(country, "");
						}
						else
						{
							strcpy(country, token);
						}

						// If a proxy server from Mainland China was read and the 
						// proxy server is not HTTP (supports SSL connections)
						if (strcmp(country, "cn") == 0 && strcmp(proxy, "http") != 0)
						{
							// Update HTTPS proxy type to HTTP which is required
							// in ProxyChains-ng configuration file syntax
							if (strcmp(proxy, "https") == 0)
							{
								strcpy(proxy, "http");
							}

							// Setup proxy server line in ProxyChains-ng
							// compatible syntax
							sprintf(temp_string, "%s %s %s", proxy, ip, port);

							// Dynamically allocate new node for Linked List
							i++;
							link = (struct proxy*)malloc(sizeof(struct proxy));
							link->line = (char*)malloc(strlen(temp_string) + 1);
							strcpy(link->line, temp_string);
							link->idx = i;
							link->num = i;
							link->next = NULL;

							// If there is no head node, this node will become the head
							if (pss->head == NULL)
							{
								pss->head = link;
								pss->current = link;
							}
							// Otherwise join the end of the Linked List
							else
							{
								pss->current->next = link;
								pss->current = link;
							}
						}
						token = ret + 1;
						ret = strchr(token, '\n');
					}
					partial_read = false;
				}

				// For each line in the chunk currently being read
				while (ret != NULL)
				{
					// Copy the next line from the downloaded file
					*ret = '\0';
					strcpy(temp_string, token);

					// Read IP Address if available
					token = strtok(temp_string, ": ");
					if (token == NULL)
					{
						strcpy(ip, "");
					}
					else
					{
						strcpy(ip, token);
					}

					// Read Port if available
					token = strtok(NULL, ": ");
					if (token == NULL)
					{
						strcpy(port, "");
					}
					else
					{
						strcpy(port, token);
					}

					// Read Proxy type if available
					token = strtok(NULL, ": ");
					if (token == NULL)
					{
						strcpy(proxy, "");
					}
					else
					{
						strcpy(proxy, token);
					}

					// Read Country if available
					token = strtok(NULL, ": ");
					if (token == NULL)
					{
						strcpy(country, "");
					}
					else
					{
						strcpy(country, token);
					}

					// If a proxy server from Mainland China was read and the 
					// proxy server is not HTTP (supports SSL connections)
					if (strcmp(country, "cn") == 0 && strcmp(proxy, "http") != 0)
					{
						// Update HTTPS proxy type to HTTP which is required
						// in ProxyChains-ng configuration file syntax
						if (strcmp(proxy, "https") == 0)
						{
							strcpy(proxy, "http");
						}

						// Setup proxy server line in ProxyChains-ng compatible syntax
						sprintf(temp_string, "%s %s %s", proxy, ip, port);

						// Dynamically allocate new node for Linked List
						i++;
						link = (struct proxy*)malloc(sizeof(struct proxy));
						link->line = (char*)malloc(strlen(temp_string) + 1);
						strcpy(link->line, temp_string);
						link->idx = i;
						link->num = i;
						link->next = NULL;

						// If there is no head node, this node will become the head
						if (pss->head == NULL)
						{
							pss->head = link;
							pss->current = link;
						}
						// Otherwise join the end of the Linked List
						else
						{
							pss->current->next = link;
							pss->current = link;
						}
					}

					// Get next line in the proxy config
					token = ret + 1;
					ret = strchr(token, '\n');
				}

				// Store the last partial line for processing when the next chunk is read
				strcpy(temp_string, token);
				partial_read = true;

				// Read contents of the downloaded file
				file_ret = lws_vfs_file_read(fop_fd, &fop_len, buf, 512);
			}

			// Close the config file
			lws_vfs_file_close(&fop_fd);

			// If no proxy servers were found
			if (pss->head == NULL)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = WARNING;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"Could not find any proxy servers located within Mainland China "
						"within downloaded proxy list.");
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] =
						SEND_NOTIFICATION;
				pss->nwa_cur++;
				pss->stopping = true;
			}
			else
			{
				// If the requested number of proxy servers exceeds what is available,
				// test all the proxy servers
				if (pss->download_num > pss->current->num)
				{
					pss->nwa_back++;
					pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = WARNING;
					sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
							"Downloaded proxy list does not contain %ld proxy servers. "
							"%d proxy servers have been found."
							, pss->download_num, pss->current->num);
					pss->next_write_action[pss->nwa_back%BUFFER_SIZE] =
							SEND_NOTIFICATION;
					pss->nwa_cur++;
					pss->download_all = true;
				}

				if (!pss->download_all)
				{
					randomise_download_list(pss, pss->current->num);
				}
				else
				{
					pss->download_num = pss->current->num;
				}
			}
			// If we are stopping, go to CLOSE state
			if (pss->stopping)
			{
				pss->state = CLOSE;
			}
			// Otherwise send progress, proxy list through WebSocket connection
			// and move onto the next step
			else
			{
				pss->progress_num = (3 * pss->download_num) + 2;

				pss->nwa_back++;
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_PROGRESS;
				pss->nwa_cur++;

				pss->current = pss->head;

				pss->nwa_back++;
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_LIST;
				pss->nwa_cur++;

				pss->state = PREP_TEMP_CONFIG_FILE;
			}

			break;

		case PREP_TEMP_CONFIG_FILE:
			// If there is nothing to do, move to the next state
			if (pss->current == NULL)
			{
				pss->state = CLOSE;
				break;
			}

			// Open the ProxyChains-ng temp config file for writing,
			// replacing the file if it exists
			flags = O_WRONLY | O_CREAT | O_TRUNC;
			fop_fd = lws_vfs_file_open(vhd->fops_plat, TEMP_PROXYCHAINSNG_CONFIG_FILE
					, &flags);

			if (!fop_fd)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"ERROR (%d): Failed to open file - %s.\n", errno
						, TEMP_PROXYCHAINSNG_CONFIG_FILE);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] =
						SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				pss->state = CLOSE;
				return;
			}

			// Write config file
			strcpy(buf, base_config);
			strcat(buf, pss->current->line);
			i = strlen(buf);
			file_ret = lws_vfs_file_write(fop_fd, &fop_len, buf
					, i);

			if (file_ret < 0)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"ERROR (%d): Failed to write file - %s.\n", errno
						, TEMP_PROXYCHAINSNG_CONFIG_FILE);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				pss->state = CLOSE;
				return;
			}

			// Close the temp config file
			lws_vfs_file_close(&fop_fd);

			// If we are stopping, go to CLOSE state
			if (pss->stopping)
			{
				pss->state = CLOSE;
			}
			// Otherwise move onto the next step
			else
			{
				pss->state = GET_PROXY_INFO;
			}

			break;

		case GET_PROXY_INFO:
			// If we are stopping, kill the current child process
			// if one has been executed
			if (pss->stopping)
			{
				if (pss->pid != 0)
				{
					kill(pss->pid, SIGKILL);
				}
			}

			// Execute child process
			lejp_construct(&pss->ctx, test_proxy_cb, pss, proxy_json
					, ARRAY_SIZE(proxy_json));
			rv = execute_process(pss->infd, pss->outfd, pss->errfd, &pss->pid
					, args_get_proxy_info_temp, &status, pss);

			if (rv < 0)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"ERROR (%d): Could not start external process.\n", errno);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				pss->state = CLOSE;
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
				pss->progress = (3 * (pss->current->idx - 1)) + pss->iteration + 1;
				pss->nwa_back++;
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_PROGRESS;
				pss->nwa_cur++;

				// Get return value, interation and send through WebSocket connection
				pss->test_information.index = pss->current->num;
				pss->test_information.return_value = WEXITSTATUS(status);
				pss->test_information.iteration = pss->iteration;

				pss->nwa_back++;
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_PROXY_INFO;
				pss->nwa_cur++;

				// If process completed successfully, move onto the next proxy server
				if (status == 0)
				{
					// Send current progress
					pss->progress = (3 * pss->current->idx) + 1;
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
					pss->current = pss->current->next;
					pss->state = PREP_TEMP_CONFIG_FILE;
				}
				// Otherwise send error message and abort
				else
				{
					// If we are stopping, go to CLOSE state
					if (pss->stopping)
					{
						pss->state = CLOSE;
					}
					else
					{
						// If the process failed, retry
						if (pss->iteration < NUM_RETRY)
						{
							pss->iteration++;
						}
						// Otherwise move onto next proxy server
						else
						{
							pss->iteration = 1;
							pss->current = pss->current->next;
							pss->state = PREP_TEMP_CONFIG_FILE;
						}
					}
				}
			}

			break;

		case CLOSE:
			// Finished the test
			pss->progress = pss->progress_num;
			pss->nwa_back++;
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_PROGRESS;
			pss->nwa_cur++;

			pss->nwa_back++;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = INFORMATION;
			if (pss->stopping)
			{
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
					"ProxyChains-ng test stopped successfully.");
			}
			else
			{
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
					"ProxyChains-ng test completed.");
			}
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
			pss->nwa_cur++;

			cleanup(pss);

			pss->state = IDLE;

			break;

		default:
			break;
	}
}

/**
 * Execute steps that need to be performed to test each proxy server configured
 * within the ProxyChains-ng configuration file based on the current state.
 * @param pss: Structure that holds variables that persist per Websocket session.
 */
void test_list(struct per_vhost_data__yi_hack_v3_test_proxy *vhd
		, struct per_session_data__yi_hack_v3_test_proxy *pss, char *buf)
{
	pid_t rv;
	int status;
	bool reached_proxy_list;
	bool partial_read;
	int i;

	lws_fop_fd_t fop_fd;
	lws_filepos_t fop_len;
	lws_fop_flags_t flags;
	int file_ret;
	char *token;
	char *ret;

	struct proxy *link;

	switch (pss->state)
	{
		case INIT:
			pss->iteration = 1;
			pss->progress = 1;

			if (pss->stopping)
			{
				pss->state = CLOSE;
			}
			else
			{
				pss->state = PARSE_FILE;
			}

			break;

		case PARSE_FILE:
			partial_read = false;
			reached_proxy_list = false;

			// Open ProxyChains-ng config file and read proxy server lines
			flags = LWS_O_RDONLY;
			fop_fd = lws_vfs_file_open(vhd->fops_plat, PROXYCHAINSNG_CONFIG_FILE,
					&flags);

			if (!fop_fd)
			{
				lwsl_err("ERROR (%d): Failed to open file - %s.\n", errno,
						PROXYCHAINSNG_CONFIG_FILE);
				pss->state = CLOSE;
				return;
			}

			// Read contents of the config file
			file_ret = lws_vfs_file_read(fop_fd, &fop_len, buf, 512);

			if (file_ret < 0)
			{
				lwsl_err("ERROR (%d): Failed to read file - %s.\n", errno,
						PROXYCHAINSNG_CONFIG_FILE);
				pss->state = CLOSE;
				return;
			}

			while (fop_len > 0)
			{
				buf[fop_len] = '\0';
				token = buf;

				ret = strchr(buf, '\n');

				// If a last line was partially read from the last chunk
				if (partial_read)
				{
					// Handle the case that we are at the last line and there are
					// no further new line characters to be read
					if (ret == NULL && fop_len < 512)
					{
						ret = strchr(buf, '\0');
					}

					if (ret != NULL)
					{
						// Join the last partial line read from the last chunk to the
						// end of the first line in the new chunk
						*ret = '\0';
						pss->current->line = (char*)realloc(pss->current->line,
								strlen(pss->current->line) + strlen(token) + 1);
						strcat(pss->current->line, token);
						token = ret + 1;
						ret = strchr(token, '\n');
					}
					partial_read = false;
				}

				// For each line in the chunk currently being read
				while (ret != NULL)
				{
					*ret = '\0';

					// If we have not reached [ProxyList], move onto next line
					if (!reached_proxy_list)
					{
						if (strcmp(token, "[ProxyList]") == 0)
						{
							reached_proxy_list = true;
							i = 0;
						}
					}
					else
					{
						// Dynamically allocate new node for Linked List
						i++;
						link = (struct proxy*)malloc(sizeof(struct proxy));
						link->line = (char*)malloc(strlen(token) + 1);
						strcpy(link->line, token);
						link->idx = i;
						link->num = i;
						link->next = NULL;

						// If there is no head node, this node will become the head
						if (pss->head == NULL)
						{
							pss->head = link;
							pss->current = link;
						}
						// Otherwise join the end of the Linked List
						else
						{
							pss->current->next = link;
							pss->current = link;
						}
					}

					// Get next line in the proxy config
					token = ret + 1;
					ret = strchr(token, '\n');
				}

				if (strlen(token) > 0)
				{
					// Dynamically allocate memory for a new node
					i++;
					link = (struct proxy*)malloc(sizeof(struct proxy));
					link->line = (char*)malloc(strlen(token) + 1);
					strcpy(link->line, token);
					link->num = i;
					link->next = NULL;

					// If there is no head node, this node will become the head
					if (pss->head == NULL)
					{
						pss->head = link;
						pss->current = link;
					}
					// Otherwise join the end of the Linked List
					else
					{
						pss->current->next = link;
						pss->current = link;
					}

					// Store the last partial line for processing when the next chunk is read
					partial_read = true;
				}

				// Read contents of the config file
				file_ret = lws_vfs_file_read(fop_fd, &fop_len, buf, 512);
			}

			// Close the config file
			lws_vfs_file_close(&fop_fd);

			// If no proxy servers were found
			if (pss->head == NULL)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = WARNING;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"Could not find any proxy servers within ProxyChains-ng "
						"configuration file.");
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] =
						SEND_NOTIFICATION;
				pss->nwa_cur++;
				pss->stopping = true;
			}

			// If we are stopping, go to CLOSE state
			if (pss->stopping)
			{
				pss->state = CLOSE;
			}
			// Otherwise send progress, proxy list through WebSocket connection
			// and move onto the next step
			else
			{
				pss->progress_num = (3 * pss->current->num) + 2;

				pss->nwa_back++;
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_PROGRESS;
				pss->nwa_cur++;

				pss->current = pss->head;

				pss->nwa_back++;
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_LIST;
				pss->nwa_cur++;

				pss->state = PREP_TEMP_CONFIG_FILE;
			}

			break;
		
		case PREP_TEMP_CONFIG_FILE:
			// If there is nothing to do, move to the next state
			if (pss->current == NULL)
			{
				pss->state = CLOSE;
				break;
			}

			// Open the ProxyChains-ng temp config file for writing,
			// replacing the file if it exists
			flags = O_WRONLY | O_CREAT | O_TRUNC;
			fop_fd = lws_vfs_file_open(vhd->fops_plat, TEMP_PROXYCHAINSNG_CONFIG_FILE
					, &flags);

			if (!fop_fd)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"ERROR (%d): Failed to open file - %s.\n", errno
						, TEMP_PROXYCHAINSNG_CONFIG_FILE);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] =
						SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				pss->state = CLOSE;
				return;
			}

			// Write config file
			strcpy(buf, base_config);
			strcat(buf, pss->current->line);
			i = strlen(buf);
			file_ret = lws_vfs_file_write(fop_fd, &fop_len, buf
					, i);

			if (file_ret < 0)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"ERROR (%d): Failed to write file - %s.\n", errno
						, TEMP_PROXYCHAINSNG_CONFIG_FILE);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] =
						SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				pss->state = CLOSE;
				return;
			}

			// Close the temp config file
			lws_vfs_file_close(&fop_fd);

			// If we are stopping, go to CLOSE state
			if (pss->stopping)
			{
				pss->state = CLOSE;
			}
			// Otherwise move onto the next step
			else
			{
				pss->state = GET_PROXY_INFO;
			}

			break;

		case GET_PROXY_INFO:
			// If we are stopping, kill the current child process
			// if one has been executed
			if (pss->stopping)
			{
				if (pss->pid != 0)
				{
					kill(pss->pid, SIGKILL);
				}
			}

			// Execute child process
			lejp_construct(&pss->ctx, test_proxy_cb, pss, proxy_json
					, ARRAY_SIZE(proxy_json));
			rv = execute_process(pss->infd, pss->outfd, pss->errfd, &pss->pid
					, args_get_proxy_info_temp, &status, pss);

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
				pss->state = CLOSE;
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
				pss->progress = (3 * (pss->current->idx - 1)) + pss->iteration + 1;
				pss->nwa_back++;
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_PROGRESS;
				pss->nwa_cur++;

				// Get return value, interation and send through WebSocket connection
				pss->test_information.index = pss->current->num;
				pss->test_information.return_value = WEXITSTATUS(status);
				pss->test_information.iteration = pss->iteration;

				pss->nwa_back++;
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_PROXY_INFO;
				pss->nwa_cur++;

				// If process completed successfully, move onto the next proxy server
				if (status == 0)
				{
					// Send current progress
					pss->progress = (3 * pss->current->idx) + 1;
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
					pss->current = pss->current->next;
					pss->state = PREP_TEMP_CONFIG_FILE;
				}
				// Otherwise send error message and abort
				else
				{
					// If we are stopping, go to CLOSE state
					if (pss->stopping)
					{
						pss->state = CLOSE;
					}
					else
					{
						// If the process failed, retry
						if (pss->iteration < NUM_RETRY)
						{
							pss->iteration++;
						}
						// Otherwise move onto next proxy server
						else
						{
							pss->iteration = 1;
							pss->current = pss->current->next;
							pss->state = PREP_TEMP_CONFIG_FILE;
						}
					}
				}
			}

			break;

		case CLOSE:
			// Finished the test
			pss->progress = pss->progress_num;
			pss->nwa_back++;
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_PROGRESS;
			pss->nwa_cur++;

			pss->nwa_back++;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = INFORMATION;
			sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
					"ProxyChains-ng test completed.");
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
			pss->nwa_cur++;

			cleanup(pss);

			pss->state = IDLE;

			break;

		default:
			break;
	}
}

/**
 * Execute steps that need to be performed to test proxy configuration
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
			pss->iteration = 1;
			pss->progress = 1;
			pss->progress_num = 8;
			pss->state = GET_INFO;

			pss->nwa_back++;
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_PROGRESS;
			pss->nwa_cur++;

			break;

		case GET_INFO:
			// Execute child process
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
				pss->state = CLOSE;
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
			// Execute child process
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
				pss->state = CLOSE;
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
	char *ptr;

	// Split the incoming data by the newline
	token = strtok((char *)in, "\n\0");

	// If nothing is found after the split, return error
	if (token == NULL)
	{
		return -1;
	}

	// Received valid request
	if (strcmp(token, "TEST_CONFIG") == 0 || strcmp(token, "TEST_LIST") == 0 ||
			strcmp(token, "DOWNLOAD_LIST") == 0 || strcmp(token, "TEST_STOP") == 0)
	{
		// Can only run one test at a time per session
		if (pss->state != IDLE && strcmp(token, "TEST_STOP") != 0)
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
			token = strtok(NULL, "\n\0");

			// If nothing is found after the split, return error
			if (token == NULL)
			{
				return -1;
			}

			// If it has been requested to download and test all proxies, set flag
			if (strcmp(token, "ALL") == 0)
			{
				pss->download_all = true;
			}
			else
			{
				// Convert string to long integer
				pss->download_num = strtol(token, &ptr, 10);

				// If the string is non-numeric...
				if (*ptr != '\0')
				{
					pss->nwa_back++;
					pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = WARNING;
					pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
					sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
							"A numeric value greater than zero must be entered.", errno);
					pss->next_write_action[pss->nwa_back%BUFFER_SIZE] =
							SEND_NOTIFICATION;
					pss->nwa_cur++;
					lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
					return -1;
				}
				// If the number is a bad value...
				else if (pss->download_num <= 0)
				{
					pss->nwa_back++;
					pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = WARNING;
					pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
					sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
							"A numeric value greater than zero must be entered.", errno);
					pss->next_write_action[pss->nwa_back%BUFFER_SIZE] =
							SEND_NOTIFICATION;
					pss->nwa_cur++;
					lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
					return -1;
				}
				else
				{
					pss->download_all = false;
				}
			}

			pss->type = DOWNLOAD_LIST;
		}
		else if (strcmp(token, "TEST_STOP") == 0)
		{
			// If in idle state, there is nothing to do
			if (pss->state != IDLE)
			{
				pss->stopping = true;
			}
			else
			{
				return -1;
			}
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
					"\"index\":\"%d\",\n\"return_value\":\"%d\",\n"
					"\"iteration\":\"%d\",\n\"ip\":\"%s\",\n\"hostname\":\"%s\",\n"
					"\"city\":\"%s\",\n\"region\":\"%s\",\n\"country\":\"%s\",\n"
					"\"loc\":\"%s\",\n\"org\":\"%s\",\n\"postal\":\"%s\"\n}"
					, pss->test_information.index, pss->test_information.return_value
					, pss->test_information.iteration, pss->test_information.ip
					, pss->test_information.hostname, pss->test_information.city
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
					"\"index\":\"%d\",\n\"return_value\":\"%d\",\n"
					"\"iteration\":\"%d\",\n\"ip\":\"%s\",\n\"hostname\":\"%s\",\n"
					"\"city\":\"%s\",\n\"region\":\"%s\",\n\"country\":\"%s\",\n"
					"\"loc\":\"%s\",\n\"org\":\"%s\",\n\"postal\":\"%s\"\n}"
					, pss->test_information.index, pss->test_information.return_value
					, pss->test_information.iteration, pss->test_information.ip
					, pss->test_information.hostname, pss->test_information.city
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
			break;

		case SEND_LIST:
			if (pss->current != NULL)
			{
				// Prepare to send data in JSON format
				n = sprintf((char *)buf, "{\n\"message\":\"SEND_LIST\",\n"
						"\"index\":\"%d\",\n\"proxy\":\"%s\"\n}"
						, pss->current->num, pss->current->line);

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
					pss->next_write_action[pss->nwa_back%BUFFER_SIZE] =
							SEND_NOTIFICATION;
					pss->nwa_cur++;
					lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
					return -1;
				}

				// Remove action from the FIFO buffer
				pss->nwa_front++;
				pss->nwa_cur--;

				// If there are more proxy servers in the list, place in FIFO buffer
				if (pss->current->next != NULL)
				{
					pss->current = pss->current->next;
					pss->nwa_back++;
					pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_LIST;
					pss->nwa_cur++;
				}
				else
				{
					pss->current = pss->head;
				}
			}
			else
			{
				// Remove nonsense action from the FIFO buffer
				pss->nwa_front++;
				pss->nwa_cur--;
			}
			break;
		
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
			vhd->fops_plat = lws_get_fops(vhd->context);

			break;

		case LWS_CALLBACK_PROTOCOL_DESTROY:
			lwsl_err("0");
			if (!vhd)
				break;

			break;

		case LWS_CALLBACK_ESTABLISHED:
			pss->wsi = wsi;
			session_init(pss);

			break;

		case LWS_CALLBACK_CLOSED:
			cleanup(pss);

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
					test_list(vhd, pss, buf);
					break;
				case DOWNLOAD_LIST:
					download_list(vhd, pss, buf);
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
