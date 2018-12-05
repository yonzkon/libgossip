#include "gossip.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	if (argc < 2) {
		printf("parameter error\n");
		return -1;
	}

	srand(time(NULL));

	char ipaddr[64];
	int port;
	sscanf(argv[1], "%63[^:]:%d", ipaddr, &port);

	char pubkey[256];
	snprintf(pubkey, 256, "gnode-%ld", random() % 100);

	struct gossip gsp = {0};
	struct gossip_node *gnode = make_gossip_node(pubkey);
	gossip_node_set_full(gnode, ipaddr, port);
	JSON_ADD_INT(gnode->data, "weight", random() % 1000);
	gnode->version++;
	gnode->update_time = time(NULL);

	assert(gossip_init(&gsp, gnode, port) == 0);
	if (argc == 3) gossip_add_seed(&gsp, argv[2]);
	gossip_run(&gsp);
	gossip_close(&gsp);
	free_gossip_node(gnode);

	return 0;
}