#include <sys/socket.h>
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

int
interact(int client_sock)
{
	int buffer = 0;
	ssize_t size = recv(client_sock, &buffer, sizeof(buffer), 0);
	if (size <= 0)
		return (int) size;
	printf("Received %d\n", buffer);
	buffer++;
	size = send(client_sock, &buffer, sizeof(buffer), 0);
	if (size > 0)
		printf("Sent %d\n", buffer);
	return (int) size;
}

void
remove_client(struct pollfd **fds, int *fd_count, int i)
{
	--*fd_count;
	struct pollfd *new_fds = malloc(*fd_count * sizeof(new_fds[0]));
	memcpy(new_fds, *fds, i * sizeof(new_fds[0]));
	memcpy(new_fds + i, *fds + i + 1,
	       (*fd_count - i) * sizeof(new_fds[0]));
	free(*fds);
	*fds = new_fds;
}

int
main(int argc, const char **argv)
{
	int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server == -1) {
		printf("error = %s\n", strerror(errno));
		return -1;
	}
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(12345);
	inet_aton("127.0.0.1", &addr.sin_addr);

	if (bind(server, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
		printf("bind error = %s\n", strerror(errno));
		return -1;
	}
	if (listen(server, 128) == -1) {
		printf("listen error = %s\n", strerror(errno));
		return -1;
	}
	struct pollfd *fds = malloc(sizeof(fds[0]));
	int fd_count = 1;
	fds[0].fd = server;
	fds[0].events = POLLIN;
	while(1) {
		int nfds = poll(fds, fd_count, 2000);
		if (nfds == 0) {
			printf("Timeout\n");
			continue;
		}
		if (nfds == -1) {
			printf("error = %s\n", strerror(errno));
			break;
		}
		if ((fds[0].revents & POLLIN) != 0) {
			int client_sock = accept(server, NULL, NULL);
			if (client_sock == -1) {
				printf("error = %s\n", strerror(errno));
				break;
			}
			printf("New client\n");
			fd_count++;
			fds = realloc(fds, fd_count * sizeof(fds[0]));
			fds[fd_count - 1].fd = client_sock;
			fds[fd_count - 1].events = POLLIN;
			nfds--;
		}
		for (int i = 0; i < fd_count && nfds > 0; ++i) {
			if ((fds[i].revents & POLLIN) == 0)
				continue;
			nfds--;
			printf("Interact with fd %d\n", fds[i].fd);
			int rc = interact(fds[i].fd);
			if (rc == -1) {
				printf("error = %s\n", strerror(errno));
				if (errno != EWOULDBLOCK && errno != EAGAIN)
					break;
			} else if (rc == 0) {
				printf("Client disconnected\n");
				remove_client(&fds, &fd_count, i);
			}
		}
	}
	for (int i = 0; i < fd_count; ++i)
		close(fds[i].fd);
	free(fds);
	close(server);
	return 0;
}
