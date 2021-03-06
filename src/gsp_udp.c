#include "gsp_udp.h"
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __WIN32
#include <winsock2.h>
#define setsockopt(A, B, C, D, E) setsockopt(A, B, C, (char *)D, E)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

int lib_init;

int gsp_udp_init(struct gsp_udp *udp, struct gsp_udp_info *info)
{
	if (!lib_init) {
#ifdef __WIN32
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
			exit(EXIT_FAILURE);
#endif
		lib_init = 1;
	}

	memset(udp, 0, sizeof(*udp));

	udp->fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (udp->fd == -1)
		return -1;

	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(info->port);
	addr.sin_addr.s_addr = inet_addr(info->ipaddr);
	if (bind(udp->fd, (struct sockaddr *)&addr, sizeof(addr))) {
		int err = errno;
		close(udp->fd);
		errno = err;
		return -1;
	}

#ifdef __WIN32
	int tv = 100;
#else
	struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 };
#endif
	if (setsockopt(udp->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		int err = errno;
		close(udp->fd);
		errno = err;
		return -1;
	}

	int buf_size = GSP_UDP_RECV_BUF_LEN_MAX;
	if (setsockopt(udp->fd, SOL_SOCKET, SO_SNDBUF,
	               &buf_size, sizeof(buf_size)) < 0) {
		int err = errno;
		close(udp->fd);
		errno = err;
		return -1;
	}
	if (setsockopt(udp->fd, SOL_SOCKET, SO_RCVBUF,
	               &buf_size, sizeof(buf_size)) < 0) {
		int err = errno;
		close(udp->fd);
		errno = err;
		return -1;
	}

	if (info->recv_buf_len < GSP_UDP_RECV_BUF_LEN_MIN)
		udp->recv_buf_len = GSP_UDP_RECV_BUF_LEN_MAX;
	else
		udp->recv_buf_len = info->recv_buf_len;

	return 0;
}

int gsp_udp_close(struct gsp_udp *udp)
{
	close(udp->fd);
	if (udp->recv_buf)
		free(udp->recv_buf);
	udp->recv_buf = NULL;
	return 0;
}

void gsp_udp_read_start(struct gsp_udp *udp, gsp_udp_read_cb read_cb)
{
	udp->ops.read_cb = read_cb;

	if (!udp->recv_buf)
		udp->recv_buf = malloc(udp->recv_buf_len);
}

void gsp_udp_read_stop(struct gsp_udp *udp)
{
	udp->ops.read_cb = NULL;
}

ssize_t gsp_udp_write(struct gsp_udp *udp, const void *buf, size_t len,
                     const struct sockaddr *addr, socklen_t addr_len)
{
	return sendto(udp->fd, buf, len, 0, addr, addr_len);
}

int gsp_udp_loop(struct gsp_udp *udp, int flags)
{
	do {
		if (udp->ops.read_cb) {
			struct sockaddr raddr = {0};
			socklen_t raddr_len = sizeof(raddr);

			ssize_t nr = recvfrom(udp->fd, udp->recv_buf,
			                      udp->recv_buf_len, 0,
			                      &raddr, &raddr_len);
			if (nr != -1) {
				udp->ops.read_cb(udp, udp->recv_buf,
				                 nr, &raddr, raddr_len);
			}
		}
	} while (flags == GSP_UDP_LOOP_FOREVER);

	return 0;
}
