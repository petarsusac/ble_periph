#ifndef _BT_H_
#define _BT_H_

#include <stdint.h>
#include <stddef.h>

#define BT_PAYLOAD_LEN (7U)

typedef void (*bt_connected_cb_t)(void);

int bt_start(bt_connected_cb_t conn_cb);
int bt_send_notification(uint8_t *data, size_t len);

#endif /* _BT_H_ */