#pragma once

enum chat_errcode {
	CHAT_ERR_INVALID_ARGUMENT = 1,
	CHAT_ERR_TIMEOUT,
	CHAT_ERR_PORT_BUSY,
	CHAT_ERR_NO_ADDR,
	CHAT_ERR_ALREADY_STARTED,
	CHAT_ERR_NOT_IMPLEMENTED,
	CHAT_ERR_NOT_STARTED,
	CHAT_ERR_SYS,
};

enum chat_events {
	CHAT_EVENT_INPUT = 1,
	CHAT_EVENT_OUTPUT = 2,
};

#define NEED_AUTHOR 1
#define NEED_SERVER_FEED 1

struct chat_message {
#ifdef NEED_AUTHOR
	/** Author's name. */
	char *author;
#endif
	/** 0-terminate text. */
	char *data;
    int size;
	/* PUT HERE OTHER MEMBERS */
};

/** Free message's memory. */
void
chat_message_delete(struct chat_message *msg);

/** Convert chat_events mask to events suitable for poll(). */
int
chat_events_to_poll_events(int mask);

void chat_make_msg(struct chat_message *msg, const char *data, int size);

/** Send message function **/
int msg_send(struct chat_message *msg, int socket);

/** Recv message function **/
int msg_recv(struct chat_message *msg, int socket);