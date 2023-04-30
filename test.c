#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define unit_test_start() \
	printf("\t-------- %s started --------\n", __func__)

#define unit_test_finish() \
	printf("\t-------- %s done --------\n", __func__)

#define unit_fail_if(cond) do {						\
	if (cond) {							\
		printf("Test failed, line %d\n", __LINE__);		\
		exit(-1);						\
	}								\
} while (0)

#define unit_msg(...) do {						\
	printf("# ");							\
	printf(__VA_ARGS__);						\
	printf("\n");							\
} while (0)

#define unit_check(cond, msg) do {					\
	if (! (cond)) {							\
		printf("not ok - %s\n", (msg));				\
		unit_fail_if(true);					\
	} else {							\
		printf("ok - %s\n", (msg));				\
	}								\
} while(0)

#include "chat.h"
#include "chat_client.h"
#include "chat_server.h"
#include "heap_help.h"

#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>

static uint16_t
server_get_port(const struct chat_server *s)
{
	int sock = chat_server_get_socket(s);
	unit_fail_if(sock < 0);
	struct sockaddr_storage addr;
	socklen_t len = sizeof(addr);
	int rc = getsockname(sock, (void *)&addr, &len);
	unit_fail_if(rc != 0);
	if (addr.ss_family == AF_INET)
		return ntohs(((struct sockaddr_in *)&addr)->sin_port);
	unit_fail_if(addr.ss_family != AF_INET6);
	return ntohs(((struct sockaddr_in6 *)&addr)->sin6_port);
}

static inline const char *
make_addr_str(uint16_t port)
{
	static char host[128];
	sprintf(host, "localhost:%u", port);
	return host;
}

static void
server_consume_events(struct chat_server *s)
{
	int rc;
	while ((rc = chat_server_update(s, 0)) == 0)
		{};
	unit_fail_if(rc != CHAT_ERR_TIMEOUT);
}

static void
client_consume_events(struct chat_client *c)
{
	int rc;
	while ((rc = chat_client_update(c, 0)) == 0)
		{};
	unit_fail_if(rc != CHAT_ERR_TIMEOUT);
}

static void
test_basic(void)
{
	unit_test_start();
	//
	// Delete the server without listen.
	//
	struct chat_server *s = chat_server_new();
	unit_check(chat_server_get_socket(s) < 0, "server no socket");
	unit_check(chat_server_get_events(s) == 0, "server no events");
	unit_check(chat_server_update(s, 0) == CHAT_ERR_NOT_STARTED,
		   "server no update");
	chat_server_delete(s);
	//
	// Delete the server right after listen.
	//
	s = chat_server_new();
	int rc = chat_server_listen(s, 0);
	unit_check(rc == 0, "listen");
	unit_check(chat_server_get_socket(s) >= 0, "server has socket");
	unit_check(server_get_port(s) > 0, "has port");
	unit_check(chat_server_get_events(s) != 0, "has events");
	chat_server_delete(s);
	//
	// Delete the client right away.
	//
	struct chat_client *c1 = chat_client_new("c1");
	unit_check(chat_client_update(c1, 0) == CHAT_ERR_NOT_STARTED,
		   "client no update");
	unit_check(chat_client_get_descriptor(c1) < 0, "client no socket");
	unit_check(chat_client_get_events(c1) == 0, "client no events");
	chat_client_delete(c1);
	//
	// Delete the client after connect.
	//
	s = chat_server_new();
	rc = chat_server_listen(s, 0);
	uint16_t port = server_get_port(s);
	c1 = chat_client_new("c1");
	unit_check(chat_server_get_events(s) == CHAT_EVENT_INPUT,
		   "server needs input");
	unit_check(chat_server_update(s, 0) == CHAT_ERR_TIMEOUT,
		   "server no clients yet");
	unit_check(chat_client_connect(c1, make_addr_str(port)) == 0,
		   "connect");
	unit_check(chat_server_update(s, 0) == 0, "server got client");
	unit_check(chat_server_get_events(s) == CHAT_EVENT_INPUT,
		   "server always needs more input");
	chat_client_delete(c1);
	unit_check(chat_server_update(s, 0) == 0, "server lost client");
	unit_check(chat_server_get_events(s) == CHAT_EVENT_INPUT,
		   "server needs more input");
	//
	// Connect a client and send a message.
	//
	c1 = chat_client_new("c1");
	unit_fail_if(chat_client_connect(c1, make_addr_str(port)) != 0);
	unit_check(chat_client_feed(c1, "hello\n", 6) == 0, "feed to client");
	client_consume_events(c1);
	server_consume_events(s);
	chat_client_delete(c1);
	struct chat_message *msg = chat_server_pop_next(s);
	unit_check(msg != NULL, "server got msg");
	unit_check(strcmp(msg->data, "hello") == 0, "msg data");
	chat_message_delete(msg);
	chat_server_delete(s);

	unit_test_finish();
}

static void
test_big_messages(void)
{
	unit_test_start();

	struct chat_server *s = chat_server_new();
	unit_fail_if(chat_server_listen(s, 0) != 0);
	uint16_t port = server_get_port(s);
	struct chat_client *c1 = chat_client_new("c1");
	unit_fail_if(chat_client_connect(c1, make_addr_str(port)) != 0);

	uint32_t len = 1024 * 1024;
	uint32_t size = len + 1;
	char *data = malloc(size);
	for (uint32_t i = 0; i < len; ++i)
		data[i] = 'a' + i % ('z' - 'a' + 1);
	data[len] = '\n';
	int rc;
	struct chat_message *msg;
	int count = 100;
	int real_count = 0;
	for (int i = 0; i < count; ++i) {
		unit_fail_if(chat_client_feed(c1, data, size) != 0);
		rc = chat_client_update(c1, 0);
		unit_fail_if(rc != 0 && rc != CHAT_ERR_TIMEOUT);
		rc = chat_server_update(s, 0);
		unit_fail_if(rc != 0 && rc != CHAT_ERR_TIMEOUT);
		while ((msg = chat_server_pop_next(s)) != NULL) {
			++real_count;
			size_t msg_len = strlen(msg->data);
			unit_fail_if(len != msg_len);
			unit_fail_if(memcmp(msg->data, data, len) != 0);
			chat_message_delete(msg);
		}
	}
	while (true) {
		int rc1 = chat_client_update(c1, 0);
		unit_fail_if(rc1 != 0 && rc1 != CHAT_ERR_TIMEOUT);
		int rc2 = chat_server_update(s, 0);
		unit_fail_if(rc2 != 0 && rc2 != CHAT_ERR_TIMEOUT);
		while ((msg = chat_server_pop_next(s)) != NULL) {
			++real_count;
			size_t msg_len = strlen(msg->data);
			unit_fail_if(len != msg_len);
			unit_fail_if(memcmp(msg->data, data, len) != 0);
			chat_message_delete(msg);
		}
		if (rc1 == CHAT_ERR_TIMEOUT && rc2 == CHAT_ERR_TIMEOUT)
			break;
	}
	free(data);
	unit_check(real_count == count, "server got all msgs");
	unit_check(chat_server_pop_next(s) == NULL, "server has no msgs");
	unit_check(chat_client_pop_next(c1) == NULL, "client has no msgs");
	chat_client_delete(c1);
	chat_server_delete(s);

	unit_test_finish();
}

int
main(void)
{
	unit_test_start();

	test_basic();
	test_big_messages();
    printf("Counter : %zu\n", heaph_get_alloc_count());

	unit_test_finish();
	return 0;
}
