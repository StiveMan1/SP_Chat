#include "chat.h"
#include "chat_client.h"

#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

struct chat_client {
	/** Socket connected to the server. */
	int socket;
    const char *name;
    /** poll **/
    int ep;
    /** Array of received messages. */
    struct chat_message *msgs;
    int ms_count;
    /** Array of received messages. */
    struct chat_message *send_msgs;
    int send_ms_count;

    struct chat_message *authors;
    int au_count;
	/* PUT HERE OTHER MEMBERS */
};



void chat_client_append_msg(struct chat_client *client) {
    if (client->msgs == NULL) {
        client->msgs = malloc(sizeof(struct chat_message));
        client->ms_count = 1;
    } else {
        client->msgs = realloc(client->msgs, sizeof(struct chat_message) * ++client->ms_count);
    }
}

void chat_client_append_send_msg(struct chat_client *client) {
    if (client->send_msgs == NULL) {
        client->send_msgs = malloc(sizeof(struct chat_message));
        client->send_ms_count = 1;
    } else {
        client->send_msgs = realloc(client->send_msgs, sizeof(struct chat_message) * ++client->send_ms_count);
    }
}

void chat_client_append_author(struct chat_client *client) {
    if (client->authors == NULL) {
        client->authors = malloc(sizeof(struct chat_message));
        client->au_count = 1;
    } else {
        client->authors = realloc(client->authors, sizeof(struct chat_message) * ++client->au_count);
    }
}

struct chat_client *
chat_client_new(const char *name)
{
	struct chat_client *client = calloc(1, sizeof(*client));
    client->socket = -1;
    client->ep = -1;

    client->name = name;

    client->msgs = NULL;
    client->ms_count = 0;

    client->send_msgs = NULL;
    client->send_ms_count = 0;

    client->authors = NULL;
    client->au_count = 0;

	return client;
}

void
chat_client_delete(struct chat_client *client)
{
	if (client->socket >= 0)
		close(client->socket);
    if (client->ep >= 0)
        close(client->ep);

    for (int i = 0; i < client->send_ms_count; i++) {
        if (client->send_msgs[i].data != NULL)
            free(client->send_msgs[i].data);
    }
    if (client->send_msgs != NULL) free(client->send_msgs);
    for (int i = 0; i < client->ms_count; i++) {
        if (client->msgs[i].data != NULL)
            free(client->msgs[i].data);
    }
    if (client->msgs != NULL) free(client->msgs);
    for (int i = 0; i < client->au_count; i++) {
        if (client->authors[i].data != NULL)
            free(client->authors[i].data);
    }
    if (client->authors != NULL) free(client->authors);

	free(client);
}

int
chat_client_connect(struct chat_client *client, const char *addr)
{
    struct sockaddr_in addr_;
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(atoi(strchr(addr, ':') + 1));
    inet_aton(addr, &addr_.sin_addr);
    addr_.sin_addr.s_addr = INADDR_ANY;
    inet_pton(AF_INET, addr, &addr_.sin_addr);

    if (client->socket != -1) {
        return CHAT_ERR_ALREADY_STARTED;
    }
    client->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client->socket == -1) {
        return CHAT_ERR_SYS;
    }

    if (connect(client->socket, (struct sockaddr *) &addr_, sizeof(addr_)) != 0) {
        return CHAT_ERR_NO_ADDR;
    }


    client->ep = epoll_create(1);
    struct epoll_event new_ev;
    new_ev.data.fd = client->socket;
    new_ev.events = EPOLLIN;
    if (epoll_ctl(client->ep, EPOLL_CTL_ADD, client->socket, &new_ev) == -1) {
        close(client->socket);
        client->socket = -1;
        return CHAT_ERR_SYS;
    }

	return 0;
}

struct chat_message *
chat_client_pop_next(struct chat_client *client)
{
    if (client->ms_count == 0) return NULL;

    struct chat_message *msg = malloc(sizeof (struct chat_message));
    struct chat_message *new_msgs = malloc(sizeof (struct chat_message) * (client->ms_count - 1));

    memcpy(new_msgs, client->msgs + 1, (client->ms_count - 1) * sizeof(struct chat_message));
    memcpy(msg, client->msgs, sizeof (struct chat_message));
    free(client->msgs);
    client->msgs = new_msgs;
    client->ms_count--;

    return msg;
}

struct chat_message *
chat_client_pop_next_send(struct chat_client *client)
{
    if (client->send_ms_count == 0) return NULL;

    struct chat_message *msg = malloc(sizeof (struct chat_message));
    struct chat_message *new_msgs = malloc(sizeof (struct chat_message) * (client->send_ms_count - 1));

    memcpy(new_msgs, client->send_msgs + 1, (client->send_ms_count - 1) * sizeof(struct chat_message));
    memcpy(msg, client->send_msgs, sizeof (struct chat_message));
    free(client->send_msgs);
    client->send_msgs = new_msgs;
    client->send_ms_count--;

    return msg;
}


int interact_client(struct chat_client *client) {
    int res;
#ifdef NEED_AUTHOR
    chat_client_append_author(client);
    client->authors[client->au_count - 1].author = NULL;
    client->authors[client->au_count - 1].data = NULL;
    client->authors[client->au_count - 1].size = 0;
    res = msg_recv(&client->authors[client->au_count - 1], client->socket);
    if (res <= 0) goto end;

    struct chat_message msg = {NULL, NULL, 0};
#else
    struct chat_message msg = {NULL, 0};
#endif

    res = msg_recv(&msg, client->socket);
    if (res <= 0) goto end;

    chat_client_append_msg(client);
#ifdef NEED_AUTHOR
    client->msgs[client->ms_count - 1].author = client->authors[client->au_count - 1].data;
#endif
    client->msgs[client->ms_count - 1].data = malloc(msg.size + 1);
    memcpy(client->msgs[client->ms_count - 1].data, msg.data, msg.size + 1);
    client->msgs[client->ms_count - 1].size = msg.size;

    end:
    if (msg.data != NULL) free(msg.data);
    return res;
}

int
chat_client_update(struct chat_client *client, double timeout)
{
    if (client->socket == -1) return CHAT_ERR_NOT_STARTED;

    struct epoll_event new_ev;
    int nfds = epoll_wait(client->ep, &new_ev, 1, (int) (timeout * 1000));
    if (nfds == 0 && client->send_ms_count == 0) return CHAT_ERR_TIMEOUT;
    if (nfds < 0 && client->send_ms_count == 0) return CHAT_ERR_SYS;

    if (nfds > 0) {
        int rc = interact_client(client);
        if (rc <= 0) {
            return CHAT_ERR_SYS;
        }
    }
    if (client->send_ms_count != 0) {
        struct chat_message *msg = chat_client_pop_next_send(client);

        if (msg->size != 0) {
            int res = msg_send(msg, client->socket);
            chat_message_delete(msg);
            if (res <= 0) return CHAT_ERR_SYS;
        } else {
            chat_message_delete(msg);
        }
    }
	return 0;
}

int
chat_client_get_descriptor(const struct chat_client *client)
{
	return client->socket;
}

int
chat_client_get_events(const struct chat_client *client)
{
    int result = 0;
    if (client->send_ms_count != 0) result |= CHAT_EVENT_OUTPUT;
    if (client->socket != -1) result |= CHAT_EVENT_INPUT;
    return result;
}

int
chat_client_feed(struct chat_client *client, const char *msg, uint32_t msg_size)
{
    if (client->socket == -1) return CHAT_ERR_NOT_STARTED;


    chat_client_append_send_msg(client);
    chat_make_msg(&client->send_msgs[client->send_ms_count - 1], msg, (int) msg_size);
#ifdef NEED_AUTHOR
    if (client->name != NULL) {
        client->send_msgs[client->send_ms_count - 1].author = (char *) client->name;
        client->name = NULL;
    } else {
        client->send_msgs[client->send_ms_count - 1].author = NULL;
    }
#endif

	return 0;
}
