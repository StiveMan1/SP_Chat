#include "chat.h"
#include "chat_server.h"

#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>

struct chat_peer {
	/** Client's socket. To read/write messages. */
	int socket;
#ifdef NEED_AUTHOR
    struct chat_message *author;
#endif
	/** Output buffer. */
	/* ... */
	/* PUT HERE OTHER MEMBERS */
};

struct chat_server {
	/** Listening socket. To accept new clients. */
	int socket;
    /** poll **/
    int ep;
    /** Array of peers. */
    struct chat_peer *peers;
    int pr_count;
    /** Array of received messages. */
    struct chat_message *msgs;
    int ms_count;

	/* PUT HERE OTHER MEMBERS */

    struct chat_message *fill_msg;
};


void chat_server_append_msg(struct chat_server *server) {
    if (server->msgs == NULL) {
        server->msgs = malloc(sizeof(struct chat_message));
        server->ms_count = 1;
    } else {
        server->msgs = realloc(server->msgs, sizeof(struct chat_message) * ++server->ms_count);
    }
}

void chat_server_append_peer(struct chat_server *server) {
    if (server->peers == NULL) {
        server->peers = malloc(sizeof(struct chat_peer));
        server->pr_count = 1;
    } else {
        server->peers = realloc(server->peers, sizeof(struct chat_peer) * ++server->pr_count);
    }
}

void chat_server_remove_client(struct chat_server *server, int fd) {
    struct chat_peer *new_peers = malloc(sizeof (struct chat_peer) * (server->pr_count - 1));
    int pr_id = -1;
    for (int i=0;i<server->pr_count;i++){
        if(server->peers[i].socket == fd) {
            pr_id = i;
            break;
        }
    }
#ifdef NEED_AUTHOR
    if (server->peers[pr_id].author != NULL)
        chat_message_delete(server->peers[pr_id].author);
#endif

    --server->pr_count;

    memcpy(new_peers, server->peers,  pr_id* sizeof(struct chat_peer));
    memcpy(new_peers + pr_id, server->peers + pr_id + 1, (server->pr_count - pr_id) * sizeof(struct chat_peer));
    free(server->peers);
    server->peers = new_peers;
}

struct chat_server *
chat_server_new(void)
{
	struct chat_server *server = calloc(1, sizeof(*server));
	server->socket = -1;
    server->ep = -1;

    server->peers = NULL;
    server->pr_count = 0;

    server->msgs = NULL;
    server->ms_count = 0;

    server->fill_msg = NULL;

	return server;
}

void
chat_server_delete(struct chat_server *server)
{
	if (server->socket >= 0)
		close(server->socket);
    if (server->ep >= 0)
        close(server->ep);
#ifdef NEED_AUTHOR
    for (int i = 0; i < server->pr_count; i++) {
        if (server->peers[i].author != NULL)
            chat_message_delete(server->peers[i].author);
    }
#endif
    if (server->peers != NULL) free(server->peers);
    for (int i = 0; i < server->ms_count; i++) {
        if (server->msgs[i].data != NULL)
            free(server->msgs[i].data);
    }
    if (server->msgs != NULL) free(server->msgs);
    if (server->fill_msg != NULL) chat_message_delete(server->fill_msg);
	free(server);
}

int
chat_server_listen(struct chat_server *server, uint16_t port)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_port = htons(port);
	/* Listen on all IPs of this machine. */
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (server->socket != -1) {
        return CHAT_ERR_ALREADY_STARTED;
    }
    server->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server->socket == -1) {
        return CHAT_ERR_SYS;
    }

    if ((bind(server->socket, (struct sockaddr *) &addr, sizeof(addr))) != 0) {
        close(server->socket);
        server->socket = -1;
        return CHAT_ERR_PORT_BUSY;
    }
    if ((listen(server->socket, 128)) < 0) {
        close(server->socket);
        server->socket = -1;
        return CHAT_ERR_PORT_BUSY;
    }

    server->ep = epoll_create(1);
    struct epoll_event new_ev;
    new_ev.data.fd = server->socket;
    new_ev.events = EPOLLIN;
    if (epoll_ctl(server->ep, EPOLL_CTL_ADD, server->socket, &new_ev) == -1) {
        close(server->socket);
        server->socket = -1;
        return CHAT_ERR_SYS;
    }

	return 0;
}

struct chat_message *
chat_server_pop_next(struct chat_server *server)
{
    if (server->ms_count == 0) return NULL;

    struct chat_message *msg = malloc(sizeof (struct chat_message));
    struct chat_message *new_msgs = malloc(sizeof (struct chat_message) * (server->ms_count - 1));

    memcpy(new_msgs, server->msgs + 1, (server->ms_count - 1) * sizeof(struct chat_message));
    memcpy(msg, server->msgs, sizeof (struct chat_message));
    free(server->msgs);
    server->msgs = new_msgs;
    server->ms_count--;

	return msg;
}

int interact_server(struct chat_server *server, int fd) {
    int res;
    struct chat_peer *peer = NULL;
    for (int i=0;i<server->pr_count && peer == NULL; i++) {
        if (fd == server->peers[i].socket) {
            peer = &server->peers[i];
        }
    }
#ifdef NEED_AUTHOR
    struct chat_message msg = {NULL, NULL, 0};
    if (peer->author == NULL) {
        peer->author = malloc(sizeof (struct chat_message));
        peer->author->data = NULL;
        peer->author->author = NULL;
        peer->author->size = 0;
        res = msg_recv(peer->author, fd);
        if (res <= 0) goto end;
    } else {
        res = msg_recv(&msg, fd);
        if (res < 0) goto end;
    }
#else
    struct chat_message msg = {NULL, 0};
#endif


    res = msg_recv(&msg, fd);
    if (res <= 0) goto end;

    chat_server_append_msg(server);
#ifdef NEED_AUTHOR
    server->msgs[server->ms_count - 1].author = peer->author->data;
#endif
    server->msgs[server->ms_count - 1].data = malloc(msg.size + 1);
    memcpy(server->msgs[server->ms_count - 1].data, msg.data, msg.size + 1);
    server->msgs[server->ms_count - 1].size = msg.size;

    for (int i = 0; i < server->pr_count; i ++)
        if (fd != server->peers[i].socket)
            msg_send(&server->msgs[server->ms_count - 1], server->peers[i].socket);

    end:
    if (msg.data != NULL) free(msg.data);
    return res;
}

int
chat_server_update(struct chat_server *server, double timeout)
{
    if (server->socket == -1) return CHAT_ERR_NOT_STARTED;

    struct epoll_event new_ev;
    int nfds = epoll_wait(server->ep, &new_ev, 1, (int) (timeout * 1000));
    if (nfds == 0) return CHAT_ERR_TIMEOUT;
    if (nfds < 0) return CHAT_ERR_SYS;

    if (nfds > 0) {
        if (new_ev.data.fd == server->socket) {
            int client_sock = accept(server->socket, NULL, NULL);
            if (client_sock == -1) return CHAT_ERR_SYS;
            new_ev.data.fd = client_sock;
            new_ev.events = EPOLLIN;
            if (epoll_ctl(server->ep, EPOLL_CTL_ADD, client_sock,
                          &new_ev) == -1) {
                return CHAT_ERR_SYS;
            }

            chat_server_append_peer(server);
            server->peers[server->pr_count - 1].socket = client_sock;
#ifdef NEED_AUTHOR
            server->peers[server->pr_count - 1].author = NULL;
#endif
        } else {
            int rc = interact_server(server, new_ev.data.fd);
            if (rc < 0) {
                return CHAT_ERR_SYS;
            } else if (rc == 0) {
                chat_server_remove_client(server, new_ev.data.fd);
                epoll_ctl(server->ep, EPOLL_CTL_DEL, new_ev.data.fd, NULL);
                close(new_ev.data.fd);
            }
        }
    }

	return 0;
}

int
chat_server_get_descriptor(const struct chat_server *server)
{
	return server->ep;
}

int
chat_server_get_socket(const struct chat_server *server)
{
	return server->socket;
}

int
chat_server_get_events(const struct chat_server *server)
{
    int result = 0;
    if (server->socket != -1) result |= CHAT_EVENT_INPUT;
	return result;
}

int
chat_server_feed(struct chat_server *server, const char *msg, uint32_t msg_size)
{
#ifdef NEED_SERVER_FEED
    if (server->socket == -1) return CHAT_ERR_NOT_STARTED;


    if (server->fill_msg == NULL) {
        server->fill_msg = malloc(sizeof(struct chat_message));
#ifdef NEED_AUTHOR
        server->fill_msg->author = NULL;
#endif
        server->fill_msg->data = malloc(msg_size + 1);
        memcpy(server->fill_msg->data, msg, msg_size);
        server->fill_msg->data[msg_size] = 0;
        server->fill_msg->size = msg_size;
    } else {
        char *new_data = malloc(server->fill_msg->size + msg_size + 1);
        memcpy(new_data, server->fill_msg->data, server->fill_msg->size);
        memcpy(new_data + server->fill_msg->size, msg, msg_size);
        free(server->fill_msg->data);
        server->fill_msg->data = new_data;
        server->fill_msg->size += msg_size;
    }

    struct chat_message *fill_msg = server->fill_msg;
    while (1) {
        int next_line = -1;
        for (int i = 0; i < fill_msg->size; i++)
            if (fill_msg->data[i] == '\n') {
                next_line = i;
                break;
            }
        if (next_line == -1) return 0;
        int start = 0, finish = next_line;
        for (; start < next_line; start++)
            if (fill_msg->data[start] != ' ' && fill_msg->data[start] != '\t' && fill_msg->data[start] != '\n') break;
        for (; start < finish; finish--)
            if (fill_msg->data[finish] != ' ' && fill_msg->data[finish] != '\t' && fill_msg->data[finish] != '\n')
                break;

        if (start != next_line) {
            struct chat_message *msg_ = malloc(sizeof(struct chat_message));
            msg_->data = malloc(finish - start + 2);
            memcpy(msg_->data, fill_msg->data + start, finish - start + 1);
            msg_->size = finish - start + 1;
            msg_->data[finish - start + 1] = 0;
#ifdef NEED_AUTHOR
            msg_->author = "server\0";
#endif
            for (int i=0;i<server->pr_count;i++)
                msg_send(msg_, server->peers[i].socket);
            chat_message_delete(msg_);
        }

        char *new_data = malloc(fill_msg->size - next_line);
        memcpy(new_data, fill_msg->data + next_line + 1, fill_msg->size - next_line);
        free(fill_msg->data);
        fill_msg->data = new_data;
        fill_msg->size -= next_line + 1;
    }
#endif
    return CHAT_ERR_NOT_IMPLEMENTED;
}
