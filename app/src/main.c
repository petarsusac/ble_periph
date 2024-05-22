#include "bt.h"

void bt_connected_cb(void)
{
	// Start sampling
}

int main(void)
{
	bt_start(bt_connected_cb);

	return 0;
}