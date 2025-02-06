#include <stdint.h>

#ifndef FLASHER_H
#define FLASHER_H

// public api
void flasher_init();
void flasher_write(uint8_t *buf, int len, uint16_t block_id);

// private api
void f_send_handshake();
void f_erase_chip();
void f_reset(int state);
void f_send_cmd(int cmd);
void f_send_addr(uint32_t addr);
void f_send_data(uint8_t *buf, int len);
void f_add_xor(uint8_t *buf, int len);
int f_check_ack();

#endif
