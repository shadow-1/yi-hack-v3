#define LWS_DLL
#define LWS_INTERNAL
#include "libwebsockets.h"

#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>

/**
 * Constants used in the plugin.
 */
#define HACK_INFO_FILE "/home/yi-hack-v3/.hackinfo"
#define BASE_VERSION_FILE "/home/homever"
#define HACK_CONFIG_FILE "/home/yi-hack-v3/etc/system.conf"
#define PROXYCHAINSNG_CONFIG_FILE "/home/yi-hack-v3/etc/proxychains.conf"
#define HOSTNAME "/home/yi-hack-v3/etc/hostname"
#define NOTIFICATION_MAX_LENGTH 200
#define BUFFER_SIZE 10
#define CHUNK_SIZE 200

/**
 * Enumeration of possible actions to be performed in the
 * libwebsockets write callback.
 */
enum lws_write_action
{
	SEND_CAM_INFO,
	SEND_CONFIG,
	SEND_PROXYCHAINSNG_CONFIG,
	SEND_NOTIFICATION
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
 * Enumeration of Notification types.
 */
enum read_action
{
	OPEN,
	APPEND,
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
 * Base ProxyChains-ng config when saving a proxy server list.
 */
char base_config[] =
	"random_chain\n\n"
	"chain_len = 1\n\n"
	"tcp_read_time_out 15000\n\n"
	"tcp_connect_time_out 8000\n\n"
	"[ProxyList]";

/**
 * Data structure that contains all variables that are stored per plugin instance.
 */
struct per_vhost_data__yi_hack_v3_info
{
	struct lws_context *context;
	struct lws_vhost *vhost;
	const struct lws_protocols *protocol;
	struct lws_plat_file_ops *fops_plat;
	unsigned char camera[32];
	unsigned char hack_version[32];
	unsigned char base_version[32];
	unsigned char hostname[32];
};

/**
 * Data structure that contains all variables that are stored per session.
 */
struct per_session_data__yi_hack_v3_info
{
	struct lws *wsi;

	enum lws_write_action next_write_action[BUFFER_SIZE];
	int nwa_back, nwa_front, nwa_cur;
	struct notification next_notification[BUFFER_SIZE];
	unsigned char proxychainsng_enabled[4];
	unsigned char httpd_enabled[4];
	unsigned char telnetd_enabled[4];
	unsigned char ftpd_enabled[4];
	unsigned char dropbear_enabled[4];
};

/**
 * Initialisation code that gets executed once upon startup.
 * @param vhd: Structure that holds variables that persist across all sessions.
 * @param buf: String buffer.
 * @return success or failure.
 */
int protocol_init(struct per_vhost_data__yi_hack_v3_info *vhd, char *buf)
{
	lws_fop_fd_t fop_fd;
	lws_filepos_t fop_len;
	lws_fop_flags_t flags;
	int file_ret;
	char *token;
	
	// Open yi-hack-v3 info file
	flags = LWS_O_RDONLY;
	fop_fd = lws_vfs_file_open(vhd->fops_plat, HACK_INFO_FILE, &flags);

	if (!fop_fd)
	{
		lwsl_err("ERROR (%d): Failed to open file - %s.\n", errno, HACK_INFO_FILE);
		return -1;
	}

	// Read contents of the info file
	file_ret = lws_vfs_file_read(fop_fd, &fop_len, buf, 512);

	if (file_ret < 0)
	{
		lwsl_err("ERROR (%d): Failed to read file - %s.\n", errno, HACK_INFO_FILE);
		return -1;
	}

	buf[fop_len] = '\0';

	// Split config file by '=' or '\n' character
	token = strtok(buf, "=\n");

	while (token != NULL)
	{
		// If "CAMERA" has been read before the '=' character
		if (strcmp(token,"CAMERA") == 0)
		{
			// Read the value that is after the '=' character
			token = strtok(NULL, "=\n");
			if (token != NULL)
			{
				strcpy(vhd->camera, token);
			}
		}
		// If "VERSION" has been read before the '=' character
		else if (strcmp(token,"VERSION") == 0)
		{
			// Read the value that is after the '=' character
			token = strtok(NULL, "=\n");
			if (token != NULL)
			{
				strcpy(vhd->hack_version, token);
			}
		}
		// Split config file by following '=' or '\n' (on the next line)
		token = strtok(NULL, "=\n");
	}

	// Close the info file
	lws_vfs_file_close(&fop_fd);

	// Open the base version file
	fop_fd = lws_vfs_file_open(vhd->fops_plat, BASE_VERSION_FILE, &flags);

	if (!fop_fd)
	{
		lwsl_err("ERROR (%d): Failed to open file - %s.\n", errno, BASE_VERSION_FILE);
		return -1;
	}	

	// Read the base version file
	file_ret = lws_vfs_file_read(fop_fd, &fop_len, buf, 512);

	if (file_ret < 0)
	{
		lwsl_err("ERROR (%d): Failed to read file - %s.\n", errno, BASE_VERSION_FILE);
		return -1;
	}

	buf[fop_len] = '\0';

	// Read the base version
	token = strtok(buf, "\n\0");
	if (token != NULL)
	{
		strcpy(vhd->base_version, token);
	}

	// Close the base version file
	lws_vfs_file_close(&fop_fd);
	
	// Open the hostname file
	fop_fd = lws_vfs_file_open(vhd->fops_plat, HOSTNAME, &flags);

	if (!fop_fd)
	{
		lwsl_err("ERROR (%d): Failed to open file - %s.\n", errno, HOSTNAME);
		return -1;
	}	

	// Read the hostname file
	file_ret = lws_vfs_file_read(fop_fd, &fop_len, buf, 512);

	if (file_ret < 0)
	{
		lwsl_err("ERROR (%d): Failed to read file - %s.\n", errno, HOSTNAME);
		return -1;
	}

	buf[fop_len] = '\0';

	// Read the hostname
	token = strtok(buf, "\n\0");
	if (token != NULL)
	{
		strcpy(vhd->hostname, token);
	}

	// Close the hostname file
	lws_vfs_file_close(&fop_fd);
	
	return 0;
}

/**
 * Initialisation code that gets executed once per Websocket session.
 * @param pss: Structure that holds variables that persist per Websocket session.
 */
void session_init(struct per_session_data__yi_hack_v3_info *pss)
{
	pss->nwa_back = -1;
	pss->nwa_front = 0;
	pss->nwa_cur = 0;

	strcpy(pss->proxychainsng_enabled, "");
	strcpy(pss->httpd_enabled, "");
	strcpy(pss->telnetd_enabled, "");
	strcpy(pss->ftpd_enabled, "");
	strcpy(pss->dropbear_enabled, "");
}

/**
 * Actions that are taken when writing to the Websocket.
 * @param wsi: Websockets instance.
 * @param vhd: Structure that holds variables that persist across all sessions.
 * @param pss: Structure that holds variables that persist per Websocket session.
 * @param buf: String buffer.
 * @return success or failure.
 */
int session_write(struct lws *wsi, struct per_vhost_data__yi_hack_v3_info *vhd,
		struct per_session_data__yi_hack_v3_info *pss, char *buf)
{
	static bool fop_first = true;
	static lws_fop_fd_t fop_fd;
	lws_filepos_t fop_len;
	lws_fop_flags_t flags;
	int file_ret;
	int n, m;
	char temp_string1[CHUNK_SIZE];
	char temp_string2[2*CHUNK_SIZE];

	// Send the next message in the FIFO action buffer
	switch (pss->next_write_action[pss->nwa_front%BUFFER_SIZE])
	{
		// Send yi-hack-v3 info via the websocket
		case SEND_CAM_INFO:
			// Prepare to send data in JSON format
			n = sprintf((char *)buf, "{\n\"message\":\"SEND_CAM_INFO\",\n\"camera\":"
					"\"%s\",\n\"base_version\":\"%s\",\n\"hack_version\":\"%s\",\n"
					"\"hostname\":\"%s\"\n}"
					,vhd->camera, vhd->base_version, vhd->hack_version, vhd->hostname);

			// Send the data
			m = lws_write(wsi, buf, n, LWS_WRITE_TEXT);
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

		// Send yi-hack-v3 config via the websocket
		case SEND_CONFIG:
			// Prepare to send data in JSON format
			n = sprintf((char *)buf, "{\n\"message\":\"SEND_CONFIG\",\n\""
					"proxychainsng_enabled\":\"%s\",\n\"httpd_enabled\":\"%s\""
					",\n\"telnetd_enabled\":\"%s\",\n\"ftpd_enabled\":\"%s\""
					",\n\"dropbear_enabled\":\"%s\"\n}"
					, pss->proxychainsng_enabled, pss->httpd_enabled
					, pss->telnetd_enabled, pss->ftpd_enabled, pss->dropbear_enabled);

			// Send the data
			m = lws_write(wsi, buf, n, LWS_WRITE_TEXT);
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

		// Send ProxyChains-ng config (progressively) via the websocket
		case SEND_PROXYCHAINSNG_CONFIG:
			// If it is the first time this action is being executed,
			// open the proxychains-ng config file
			if (fop_first)
			{
				flags = LWS_O_RDONLY;
				fop_fd = lws_vfs_file_open(vhd->fops_plat, PROXYCHAINSNG_CONFIG_FILE
						, &flags);

				if (!fop_fd) {
					pss->nwa_back++;
					pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
					pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
					sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message
							, "ERROR (%d): Failed to open file - %s.\n"
							, errno, PROXYCHAINSNG_CONFIG_FILE);
					pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
					pss->nwa_cur++;
					lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
					return -1;
				}

				fop_first = false;
			}

			// Read a chunk from the config file
			file_ret = lws_vfs_file_read(fop_fd, &fop_len, temp_string1, CHUNK_SIZE);

			if (file_ret < 0)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message
						, "ERROR (%d): Failed to read file - %s.\n"
						, errno, PROXYCHAINSNG_CONFIG_FILE);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				return -1;
			}

			temp_string1[fop_len] = '\0';

			lws_json_purify(temp_string2, temp_string1, 2*CHUNK_SIZE);

			// Prepare to send chunk in JSON format
			n = sprintf((char *)buf, "{\n\"message\":\"SEND_PROXYCHAINSNG_CONFIG\",\n\""
					"config\":\"%s\"\n}", temp_string2);

			// Send the data
			m = lws_write(wsi, buf, n, LWS_WRITE_TEXT);
			if (m < n) {
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message
						, "ERROR (%d): Error writing to yi-hack-v3_info Websocket.\n"
						, errno);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				return -1;
			}

			// If this is the last chunk to read,
			// close the file and remove action from FIFO buffer
			if (fop_len < CHUNK_SIZE) {
				lws_vfs_file_close(&fop_fd);

				fop_first = true;

				pss->nwa_front++;
				pss->nwa_cur--;
			}
			break;

		// Send notification via the websocket
		case SEND_NOTIFICATION:
			// Prepare to send data in JSON format
			n = sprintf((char *)buf, "{\n\"message\":\"SEND_NOTIFICATION\",\n\""
					"n_type\":\"%d\",\n\"n_number\":\"%d\",\n\"n_message\":\"%s\"\n}"
					, pss->next_notification[pss->nwa_front%BUFFER_SIZE].nt
					, pss->next_notification[pss->nwa_front%BUFFER_SIZE].code
					, pss->next_notification[pss->nwa_front%BUFFER_SIZE].message);

			// Send the data
			m = lws_write(wsi, buf, n, LWS_WRITE_TEXT);
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

		// For all other requests, do nothing
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
int session_read(struct per_vhost_data__yi_hack_v3_info *vhd,
		struct per_session_data__yi_hack_v3_info *pss, char *in, char *buf)
{
	static lws_fop_fd_t fop_fd;
	lws_filepos_t fop_len;
	lws_fop_flags_t flags;
	int file_ret;
	char *token;
	int n, m;

	// Split the incoming data by the newline
	token = strtok((char *)in, "\n\0");

	// If nothing is found after the split, return error
	if (token == NULL)
	{
		return -1;
	}

	// Received request for yi-hack-v3 info
	if (strcmp(token, "SEND_CAM_INFO") == 0)
	{
		// Add request to the FIFO buffer
		pss->nwa_back++;
		pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_CAM_INFO;
		pss->nwa_cur++;
	}
	// Received request for yi-hack-v3 config
	else if (strcmp(token, "SEND_CONFIG") == 0)
	{
		// Open the yi-hack-v3 config file
		flags = LWS_O_RDONLY;
		fop_fd = lws_vfs_file_open(vhd->fops_plat, HACK_CONFIG_FILE, &flags);

		if (!fop_fd)
		{
			pss->nwa_back++;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
			sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
					"ERROR (%d): Failed to open file - %s.\n", errno, HACK_CONFIG_FILE);
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
			pss->nwa_cur++;
			lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
			return -1;
		}

		// Read the config file
		file_ret = lws_vfs_file_read(fop_fd, &fop_len, buf, 512);

		if (file_ret < 0)
		{
			pss->nwa_back++;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
			sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
					"ERROR (%d): Failed to read file - %s.\n", errno, HACK_CONFIG_FILE);
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
			pss->nwa_cur++;
			lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
			return -1;
		}

		buf[fop_len] = '\0';

		// Split config file by '=' or '\n' character
		token = strtok(buf, "=\n");

		while (token != NULL)
		{
			// If "PROXYCHAINSNG" has been read before the '=' character
			if (strcmp(token,"PROXYCHAINSNG") == 0)
			{
				// Read the value that is after the '=' character
				token = strtok(NULL, "=\n");
				if (token != NULL)
				{
					// If "yes" or "no" were read, store the value
					if (strcmp(token, "yes") || strcmp(token, "no"))
						strcpy(pss->proxychainsng_enabled, token);
				}
			}
			// If "HTTPD" has been read before the '=' character
			else if (strcmp(token,"HTTPD") == 0)
			{
				// Read the value that is after the '=' character
				token = strtok(NULL, "=\n");
				if (token != NULL)
				{
					// If "yes" or "no" were read, store the value
					if (strcmp(token, "yes") || strcmp(token, "no"))
						strcpy(pss->httpd_enabled, token);
				}
			}
			// If "TELNETD" has been read before the '=' character
			else if (strcmp(token,"TELNETD") == 0)
			{
				// Read the value that is after the '=' character
				token = strtok(NULL, "=\n");
				if (token != NULL)
				{
					// If "yes" or "no" were read, store the value
					if (strcmp(token, "yes") || strcmp(token, "no"))
						strcpy(pss->telnetd_enabled, token);
				}
			}
			// If "FTPD" has been read before the '=' character
			else if (strcmp(token,"FTPD") == 0)
			{
				// Read the value that is after the '=' character
				token = strtok(NULL, "=\n");
				if (token != NULL)
				{
					// If "yes" or "no" were read, store the value
					if (strcmp(token, "yes") || strcmp(token, "no"))
						strcpy(pss->ftpd_enabled, token);
				}
			}
			// If "DROPBEAR" has been read before the '=' character
			else if (strcmp(token,"DROPBEAR") == 0)
			{
				// Read the value that is after the '=' character
				token = strtok(NULL, "=\n");
				if (token != NULL)
				{
					// If "yes" or "no" were read, store the value
					if (strcmp(token, "yes") || strcmp(token, "no"))
						strcpy(pss->dropbear_enabled, token);
				}
			}
			// Split config file by following '=' or '\n' (on the next line)
			token = strtok(NULL, "=\n");
		}
		
		// Close the config file
		lws_vfs_file_close(&fop_fd);

		// Add action to the FIFO buffer
		pss->nwa_back++;
		pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_CONFIG;
		pss->nwa_cur++;
	}
	// Received request to save yi-hack-v3 config
	else if (strcmp(token, "SAVE_CONFIG") == 0)
	{
		// Read the value that is before the '=' character
		token = strtok(NULL, "=\n");
		while (token != NULL)
		{
			// If proxychains-ng enabled is being read
			if (strcmp(token,"proxychainsng_enabled") == 0)
			{
				// Read the value that is after the '=' character
				token = strtok(NULL, "=\n");
				if (token != NULL)
				{
					// If "yes" or "no" was read, store the value
					if (strcmp(token, "yes") || strcmp(token, "no"))
						strcpy(pss->proxychainsng_enabled, token);
				}
			}
			// If httpd enabled is being read
			else if (strcmp(token,"httpd_enabled") == 0)
			{
				// Read the value that is after the '=' character
				token = strtok(NULL, "=\n");
				if (token != NULL)
				{
					// If "yes" or "no" was read, store the value
					if (strcmp(token, "yes") || strcmp(token, "no"))
						strcpy(pss->httpd_enabled, token);
				}
			}
			// If telnetd enabled is being read
			else if (strcmp(token,"telnetd_enabled") == 0)
			{
				// Read the value that is after the '=' character
				token = strtok(NULL, "=\n");
				if (token != NULL)
				{
					// If "yes" or "no" was read, store the value
					if (strcmp(token, "yes") || strcmp(token, "no"))
						strcpy(pss->telnetd_enabled, token);
				}
			}
			// If ftpd enabled is being read
			else if (strcmp(token,"ftpd_enabled") == 0)
			{
				// Read the value that is after the '=' character
				token = strtok(NULL, "=\n");
				if (token != NULL)
				{
					// If "yes" or "no" was read, store the value
					if (strcmp(token, "yes") || strcmp(token, "no"))
						strcpy(pss->ftpd_enabled, token);
				}
			}
			// If dropbear enabled is being read
			else if (strcmp(token,"dropbear_enabled") == 0)
			{
				// Read the value that is after the '=' character
				token = strtok(NULL, "=\n");
				if (token != NULL)
				{
					// If "yes" or "no" was read, store the value
					if (strcmp(token, "yes") || strcmp(token, "no"))
						strcpy(pss->dropbear_enabled, token);
				}
			}
			// If the hostname is being read
			else if (strcmp(token,"hostname") == 0)
			{
				// Read the value that is after the '=' character
				token = strtok(NULL, "\n");
				if (token != NULL)
				{
					if (strlen(token) < 32)
					{
						strcpy(vhd->hostname, token);
					}
					else
					{
						pss->nwa_back++;
						pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = WARNING;
						sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
								"Hostname has to be less than 32 characters long. "
								"Hostname not saved.");
						pss->next_write_action[pss->nwa_back%BUFFER_SIZE] =
								SEND_NOTIFICATION;
						pss->nwa_cur++;
					}
				}
			}
			// Split config file by following '=' or '\n' (on the next line)
			token = strtok(NULL, "=\n");
		}

		// Open config file for writing, replacing the file if it exists
		flags = O_WRONLY | O_TRUNC;
		fop_fd = lws_vfs_file_open(vhd->fops_plat, HACK_CONFIG_FILE, &flags);

		if (!fop_fd)
		{
			pss->nwa_back++;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
			sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
					"ERROR (%d): Failed to open file - %s.\n", errno, HACK_CONFIG_FILE);
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
			pss->nwa_cur++;
			lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
			return -1;
		}

		// Prepare to save config file
		n = sprintf((char *)buf, "PROXYCHAINSNG=%s\nHTTPD=%s\nTELNETD=%s\nFTPD=%s"
				"\nDROPBEAR=%s"
				, pss->proxychainsng_enabled, pss->httpd_enabled, pss->telnetd_enabled
				, pss->ftpd_enabled, pss->dropbear_enabled);

		if (n != strlen(buf))
		{
			pss->nwa_back++;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
			sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
					"ERROR (%d): Failed to prepare writing to file - %s.\n", errno
					, BASE_VERSION_FILE);
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
			pss->nwa_cur++;
			lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
			return -1;
		}

		// Write config file
		file_ret = lws_vfs_file_write(fop_fd, &fop_len, buf, n);

		if (file_ret < 0)
		{
			pss->nwa_back++;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
			sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
					"ERROR (%d): Failed to write file - %s.\n", errno, HACK_CONFIG_FILE);
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
			pss->nwa_cur++;
			lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
			return -1;
		}

		// Close config file
		lws_vfs_file_close(&fop_fd);

		// Open hostname file for writing, replacing the file if it exists
		flags = O_WRONLY | O_TRUNC;
		fop_fd = lws_vfs_file_open(vhd->fops_plat, HOSTNAME, &flags);

		if (!fop_fd)
		{
			pss->nwa_back++;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
			sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
					"ERROR (%d): Failed to open file - %s.\n", errno, HOSTNAME);
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
			pss->nwa_cur++;
			lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
			return -1;
		}

		// Prepare to save hostname file
		n = sprintf((char *)buf, "%s", vhd->hostname);

		if (n != strlen(buf))
		{
			pss->nwa_back++;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
			sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
					"ERROR (%d): Failed to prepare writing to file - %s.\n", errno
					, HOSTNAME);
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
			pss->nwa_cur++;
			lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
			return -1;
		}

		// Write config file
		file_ret = lws_vfs_file_write(fop_fd, &fop_len, buf, n);

		if (file_ret < 0)
		{
			pss->nwa_back++;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
			sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
					"ERROR (%d): Failed to write file - %s.\n", errno, HOSTNAME);
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
			pss->nwa_cur++;
			lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
			return -1;
		}

		// Close hostname file
		lws_vfs_file_close(&fop_fd);
		
		pss->nwa_back++;
		pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = INFORMATION;
		sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
				"Settings applied. Reboot is required for changes to take effect.");
		pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
		pss->nwa_cur++;
	}
	// Received request for proxychains-ng config
	else if (strcmp(token, "SEND_PROXYCHAINSNG_CONFIG") == 0)
	{
		// Add request to action FIFO buffer
		pss->nwa_back++;
		pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_PROXYCHAINSNG_CONFIG;
		pss->nwa_cur++;
	}
	// Received request to save proxychains-ng config
	else if (strcmp(token, "SAVE_PROXYCHAINSNG_CONFIG") == 0)
	{
		enum read_action current_read_action;

		// Split the incoming data by the newline
		token = strtok(NULL, "\n\0");

		// If nothing is found after the split, return error
		if (token == NULL)
		{
			return -1;
		}

		if (strcmp(token, "OPEN") == 0)
			current_read_action = OPEN;
		else if (strcmp(token, "APPEND") == 0)
			current_read_action = APPEND;
		else
			current_read_action = CLOSE;

		if (current_read_action == OPEN)
		{
			// Open the proxychains-ng config file for writing,
			// replacing the file if it exists
			flags = O_WRONLY | O_TRUNC;
			fop_fd = lws_vfs_file_open(vhd->fops_plat, PROXYCHAINSNG_CONFIG_FILE, &flags);

			if (!fop_fd)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"ERROR (%d): Failed to open file - %s.\n", errno
						, PROXYCHAINSNG_CONFIG_FILE);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				return -1;
			}
		}

		token = strtok(NULL, "");

		if (token != NULL)
		{
			n = strlen(token);

			// Write config file
			file_ret = lws_vfs_file_write(fop_fd, &fop_len, token, n);

			if (file_ret < 0)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"ERROR (%d): Failed to write file - %s.\n", errno
						, PROXYCHAINSNG_CONFIG_FILE);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				return -1;
			}
		}

		if (current_read_action == CLOSE)
		{
			// Close the file and add notification to the FIFO buffer
			lws_vfs_file_close(&fop_fd);

			pss->nwa_back++;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = INFORMATION;
			sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message
					, "Configuration saved.");
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
			pss->nwa_cur++;
		}
	}
	// Received request to save proxychains-ng list of proxy servers
	else if (strcmp(token, "SAVE_PROXYCHAINSNG_LIST") == 0)
	{
		enum read_action current_read_action;

		// Split the incoming data by the newline
		token = strtok(NULL, "\n\0");

		// If nothing is found after the split, return error
		if (token == NULL)
		{
			return -1;
		}

		if (strcmp(token, "OPEN") == 0)
			current_read_action = OPEN;
		else if (strcmp(token, "APPEND") == 0)
			current_read_action = APPEND;
		else
			current_read_action = CLOSE;

		if (current_read_action == OPEN)
		{
			// Open the proxychains-ng config file for writing,
			// replacing the file if it exists
			flags = O_WRONLY | O_TRUNC;
			fop_fd = lws_vfs_file_open(vhd->fops_plat, PROXYCHAINSNG_CONFIG_FILE, &flags);

			if (!fop_fd)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"ERROR (%d): Failed to open file - %s.\n", errno
						, PROXYCHAINSNG_CONFIG_FILE);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				return -1;
			}

			// Write default initial portion of the ProxyChains-ng config file
			n = strlen(base_config);
			file_ret = lws_vfs_file_write(fop_fd, &fop_len, base_config, n);

			if (file_ret < 0)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"ERROR (%d): Failed to write file - %s.\n", errno
						, PROXYCHAINSNG_CONFIG_FILE);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				return -1;
			}
		}

		token = strtok(NULL, "");

		if (token != NULL)
		{
			n = strlen(token);

			// Write config file
			file_ret = lws_vfs_file_write(fop_fd, &fop_len, "\n", 1);
			file_ret = lws_vfs_file_write(fop_fd, &fop_len, token, n);

			if (file_ret < 0)
			{
				pss->nwa_back++;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = ERROR;
				pss->next_notification[pss->nwa_back%BUFFER_SIZE].code = errno;
				sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message,
						"ERROR (%d): Failed to write file - %s.\n", errno
						, PROXYCHAINSNG_CONFIG_FILE);
				pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
				pss->nwa_cur++;
				lwsl_err(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message);
				return -1;
			}
		}

		if (current_read_action == CLOSE)
		{
			// Close the file and add notification to the FIFO buffer
			lws_vfs_file_close(&fop_fd);

			pss->nwa_back++;
			pss->next_notification[pss->nwa_back%BUFFER_SIZE].nt = INFORMATION;
			sprintf(pss->next_notification[pss->nwa_back%BUFFER_SIZE].message
					, "Configuration saved.");
			pss->next_write_action[pss->nwa_back%BUFFER_SIZE] = SEND_NOTIFICATION;
			pss->nwa_cur++;
		}
	}
}

/**
 * yi-hack-v3_info libwebsockets plugin callback function.
 * @param wsi: Websockets instance.
 * @param reason: The reason why the callback has been executed.
 * @param user: Per session data.
 * @param in: String that contains data sent via Websocket connection.
 * @param len: Length of data sent via Websocket connection.
 * @return success or failure.
 */
static int
callback_yi_hack_v3_info(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	struct per_session_data__yi_hack_v3_info *pss =
				(struct per_session_data__yi_hack_v3_info *)user;
	struct per_vhost_data__yi_hack_v3_info *vhd =
				(struct per_vhost_data__yi_hack_v3_info *)lws_protocol_vh_priv_get
				(lws_get_vhost(wsi), lws_get_protocol(wsi));
	unsigned char buf[LWS_PRE + 512];
	unsigned char *p = &buf[LWS_PRE];

	switch (reason)
	{
		case LWS_CALLBACK_PROTOCOL_INIT:
			vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi)
					, lws_get_protocol(wsi)
					, sizeof(struct per_vhost_data__yi_hack_v3_info));
			vhd->context = lws_get_context(wsi);
			vhd->protocol = lws_get_protocol(wsi);
			vhd->vhost = lws_get_vhost(wsi);
			vhd->fops_plat = lws_get_fops(vhd->context);

			protocol_init(vhd, p);

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
				session_write(wsi, vhd, pss, p);

				if (pss->nwa_cur > 0)
					lws_callback_on_writable(pss->wsi);
			}

			break;

		case LWS_CALLBACK_RECEIVE:
			session_read(vhd, pss, (char *)in, p);

			lws_callback_on_writable(pss->wsi);
			break;

		default:
			break;
		}

		return 0;
}

static const struct lws_protocols protocols[] = {
	{
		"yi-hack-v3_info",
		callback_yi_hack_v3_info,
		sizeof(struct per_session_data__yi_hack_v3_info),
		1024, /* rx buf size must be >= permessage-deflate rx size */
	},
};

LWS_VISIBLE int
init_protocol_yi_hack_v3_info(struct lws_context *context,
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
destroy_protocol_yi_hack_v3_info(struct lws_context *context)
{
	return 0;
}
