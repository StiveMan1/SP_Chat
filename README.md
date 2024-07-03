# Game lobby chat.
## Language: C.

Need to implement a game lobby chat. It consists of a chat server
and a client.

The clients connect to the server and each message is broadcasted
to all the clients via this server. So the server is like a game
lobby. Everyone reads all messages from all the others and the
messages are not persisted anyhow.

In the attached .h and .c files you can find templates of
functions and structures which need to be implemented.

A usage example of the exe files is that you start a server,
start one or more clients, and everything sent by anybody is
displayed in all clients. Like so:

```shell
$> ./server             $> ./client             $> ./client
I am client-1!          I am client-1!          I am client-1!
I am client-2!          I am client-2!          I am client-2!
```

Rules:

- Message end is `\n` (line wrap).

- Own messages are not sent back to the client.

- Empty messages (consisting of just spaces - see isspace()
  function) are not sent at all.

- Each message is trimmed from any spaces at both left and right
  sides. For example, if I type "  m s   g   " in a terminal and
  press enter, the message to be sent should be "m s   g".