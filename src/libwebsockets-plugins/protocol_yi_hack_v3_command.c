#define LWS_DLL
#define LWS_INTERNAL
#include "libwebsockets.h"

#include <string.h>

/**
 * Data structure that contains all variables that are stored per plugin instance.
 */
struct per_vhost_data__yi_hack_v3_command
{
	struct lws_context *context;
	struct lws_vhost *vhost;
	const struct lws_protocols *protocol;
};

/**
 * Data structure that contains all variables that are stored per session.
 */
struct per_session_data__yi_hack_v3_command
{
	// For future use
};

/**
 * Actions that are taken when reading from the Websocket.
 * @param vhd: Structure that holds variables that persist across all sessions.
 * @param pss: Structure that holds variables that persist per Websocket session.
 * @param in: Data that has been received from the Websocket session.
 * @param buf: String buffer.
 * @return success or failure.
 */
int session_read(struct per_vhost_data__yi_hack_v3_command *vhd
		, struct per_session_data__yi_hack_v3_command *pss, char *in, char *buf)
{
	char *token;

	// Split the incoming data by the newline
	token = strtok((char *)in, "\n\0");

	// If nothing is found after the split, return error
	if (token == NULL)
	{
		return -1;
	}

	// Received request to reboot the camera
	if (strcmp(token, "REBOOT") == 0)
	{
		system("reboot");
	}

	return 0;
}

/**
 * yi-hack-v3_command libwebsockets plugin callback function.
 * @param wsi: Websockets instance.
 * @param reason: The reason why the callback has been executed.
 * @param user: Per session data.
 * @param in: String that contains data sent via Websocket connection.
 * @param len: Length of data sent via Websocket connection.
 * @return success or failure.
 */
static int
callback_yi_hack_v3_command(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	struct per_session_data__yi_hack_v3_command *pss =
				(struct per_session_data__yi_hack_v3_command *)user;
	struct per_vhost_data__yi_hack_v3_command *vhd =
				(struct per_vhost_data__yi_hack_v3_command *)lws_protocol_vh_priv_get
				(lws_get_vhost(wsi), lws_get_protocol(wsi));
	unsigned char buf[LWS_PRE + 512];
	unsigned char *p = &buf[LWS_PRE];

	switch (reason)
	{
		case LWS_CALLBACK_PROTOCOL_INIT:
			vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi), lws_get_protocol(wsi)
					, sizeof(struct per_vhost_data__yi_hack_v3_command));
			vhd->context = lws_get_context(wsi);
			vhd->protocol = lws_get_protocol(wsi);
			vhd->vhost = lws_get_vhost(wsi);

			break;

		case LWS_CALLBACK_PROTOCOL_DESTROY:
			if (!vhd)
				break;

			break;

		case LWS_CALLBACK_RECEIVE:
			session_read(vhd, pss, (char *)in, p);

			break;

		default:
			break;
		}

		return 0;
}

static const struct lws_protocols protocols[] = {
	{
		"yi-hack-v3_command",
		callback_yi_hack_v3_command,
		sizeof(struct per_session_data__yi_hack_v3_command),
		1024, /* rx buf size must be >= permessage-deflate rx size */
	},
};

LWS_VISIBLE int
init_protocol_yi_hack_v3_command(struct lws_context *context,
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
destroy_protocol_yi_hack_v3_command(struct lws_context *context)
{
	return 0;
}
