#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
// #include <sys/socket.h>
// #include <sys/select.h>
// #include <netinet/in.h>

typedef struct		s_client
{
	int				fd;
	int 			id;
	struct s_client	*next;
}	t_client;

t_client *g_clients = NULL;

int sock_fd, g_id = 0;
fd_set curr_sock, cpy_read, cpy_write;
char msg[42*4096], buf[42*4096 + 42];

void fatal()
{
	write(2, "Fatal error\n", strlen("Fatal error\n"));
	close(sock_fd);
	exit(1);
}

int get_id(int fd)
{
	t_client *tmp = g_clients;

	while (tmp)
	{
		if (tmp->fd == fd)
			return (tmp->id);
		tmp = tmp->next;
	}
	return (-1);
}

int get_max_fd()
{
	int	max = sock_fd;
	t_client *tmp = g_clients;

	while (tmp)
	{
		if (tmp->fd > max)
			max = tmp->fd;
		tmp = tmp->next;
	}
	return (max);
}

void send_all(int fd, char *str_req)
{
	t_client *tmp = g_clients;

	while (tmp)
	{
		if (tmp->fd != fd && FD_ISSET(tmp->fd, &cpy_write))
		{
			if (send(tmp->fd, str_req, strlen(str_req), 0) < 0)
				fatal();
		}
		tmp = tmp->next;
	}
}

int add_client_to_list(int fd)
{
	t_client *tmp = g_clients;
	t_client *new;

	if (!(new = calloc(1, sizeof(t_client))))
		fatal();
	new->id = g_id++;
	new->fd = fd;
	new->next = NULL;
	if (!g_clients)
	{
		g_clients = new;
	}
	else
	{
		while (tmp->next)
			tmp = tmp->next;
		tmp->next = new;
	}
	return (new->id);
}

void add_client()
{
	struct sockaddr_in clientaddr;
	socklen_t len = sizeof(clientaddr);
	int client_fd;

	if ((client_fd = accept(sock_fd, (struct sockaddr *)&clientaddr, &len)) < 0)
		fatal();
	sprintf(buf, "server: client %d just arrived\n", add_client_to_list(client_fd));
	send_all(client_fd, buf);
	FD_SET(client_fd, &curr_sock);
}

int rm_client(int fd)
{
	t_client *tmp = g_clients;
	t_client *del;
	int id = get_id(fd);

	if (tmp && tmp->fd == fd)
	{
		g_clients = tmp->next;
		free(tmp);
	}
	else
	{
		while (tmp && tmp->next && tmp->next->fd != fd)
			tmp = tmp->next;
		del = tmp->next;
		tmp->next = tmp->next->next;
		free(del);
	}
	return (id);
}

void ex_msg(int fd)
{
	int i = 0;
	int j = 0;
	char tmp[42*4096];

	bzero(&tmp, sizeof(tmp));
	while (msg[i])
	{
		tmp[j] = msg[i];
		j++;
		if (msg[i] == '\n')
		{
			sprintf(buf, "client %d: %s", get_id(fd), tmp);
			send_all(fd, buf);
			j = 0;
			bzero(&tmp, strlen(tmp));
			bzero(&buf, strlen(buf));
		}
		i++;
	}
	bzero(&msg, strlen(msg));
}

int main(int ac, char **av)
{
	if (ac != 2)
	{
		write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
		exit(1);
	}

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1]));

	if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		fatal();
	if (bind(sock_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
		fatal();
	if (listen(sock_fd, 0) < 0)
		fatal();

	FD_ZERO(&curr_sock);
	FD_SET(sock_fd, &curr_sock);
	bzero(&buf, sizeof(buf));
	bzero(&msg, sizeof(msg));
	while (1)
	{
		cpy_write = cpy_read = curr_sock;
		if (select(get_max_fd() + 1, &cpy_read, &cpy_write, NULL, NULL) < 0)
			continue;
		for (int fd = 0; fd <= get_max_fd(); fd++)
		{
			if (FD_ISSET(fd, &cpy_read))
			{
				if (fd == sock_fd)
				{
					bzero(&buf, sizeof(buf));
					add_client();
					break;
				}
				else
				{
					int ret_recv = 1000;
					while (ret_recv == 1000 || msg[strlen(msg) - 1] != '\n')
					{
						ret_recv = recv(fd, msg + strlen(msg), 1000, 0);
						if (ret_recv <= 0)
							break ;
					}
					if (ret_recv <= 0)
					{
						bzero(&buf, sizeof(buf));
						sprintf(buf, "server: client %d just left\n", rm_client(fd));
						send_all(fd, buf);
						FD_CLR(fd, &curr_sock);
						close(fd);
						break;
					}
					else
						ex_msg(fd);
				}
			}

		}

	}
	return (0);
}
