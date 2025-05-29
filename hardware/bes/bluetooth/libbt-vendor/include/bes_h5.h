#ifndef __BES_H5_H__
#define __BES_H5_H__

void h5_init(void);
void h5_close(void);
bool bes_h5_recv(uint8_t *data, int count);
uint16_t h5_transmit_data_to_btc(uint8_t *data, uint16_t total_length);
#endif

