#include "chat.h"

#include <poll.h>
#include <stdlib.h>
#include <sys/un.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <poll.h>

void
chat_message_delete(struct chat_message *msg)
{
	free(msg->data);
	free(msg);
}

int
chat_events_to_poll_events(int mask)
{
	int res = 0;
	if ((mask & CHAT_EVENT_INPUT) != 0)
		res |= POLLIN;
	if ((mask & CHAT_EVENT_OUTPUT) != 0)
		res |= POLLOUT;
	return res;
}

void chat_make_msg(struct chat_message *msg, const char *data, int size) {
    int start = 0;
    int finish = (int) size;
    for (; start <= finish; start ++) {
        if (data[start] != 0 && data[start] != '\t' && data[start] != '\n' && data[start] != ' ') break;
    }
    for (; start < finish; finish--) {
        if (data[finish - 1] != 0 && data[finish - 1] != '\t' && data[finish - 1] != '\n' && data[finish - 1] != ' ') break;
    }
    msg->data = malloc(finish - start + 1);
    memcpy(msg->data, data, finish - start);
    msg->data[finish - start] = 0;
    msg->size = finish - start;
}

int msg_send(struct chat_message *msg, int socket) {
#ifdef NEED_AUTHOR
    {
        char size_buf[] = {0, 0, 0, 0};
        if (msg->author != NULL) {
            for (int i = (int) strlen(msg->author), j = 0; i != 0 && j < 4; i >>= 8, j++) {
                size_buf[j] = (char) (i % 256);
            }
        }

        int res = (int) send(socket, size_buf, 4, 0);
        if (res <= 0) return res;

        if (msg->author != NULL) {
            size_t size = 0;
            while (size != strlen(msg->author)) {
                res = (int) send(socket, msg->author + size, strlen(msg->author) - size, 0);
                if (res <= 0) return res;
                size += res;
            }
        }
    }
#endif

    char size_buf[] = {0, 0, 0, 0};
    for (int i = msg->size, j = 0; i != 0 && j < 4; i >>= 8, j++) {
        size_buf[j] = (char) (i % 256);
    }
    int res = (int) send(socket, size_buf, 4, 0);
    if (res <= 0) return res;

    int size = 0;
    while (size != msg->size) {
        res = (int) send(socket, msg->data + size, msg->size - size, 0);
        if (res <= 0) return res;
        size += res;
    }
    return size;
}

int msg_recv(struct chat_message *msg, int socket) {
    char size_buf[] = {0, 0, 0, 0};

    int res = (int) recv(socket, size_buf, 4, 0);
    if (res <= 0) return (int) res;

    if (msg->data != NULL) free(msg->data);

    msg->size = 0;
    for (int i = 3; i >= 0; i--) {
        msg->size <<= 8;
        msg->size += size_buf[i];
    }
    msg->data = malloc(msg->size + 1);
    msg->data[msg->size] = 0;

    int size = 0;
    while (size != msg->size) {
        res = (int) recv(socket, msg->data + size, msg->size - size, 0);
        if (res <= 0) return res;
        size += res;
    }
    return size;
}
