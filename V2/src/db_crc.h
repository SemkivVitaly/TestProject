/**
 * db_crc.h — CRC (порт из DroneBridge). DVB-S2 для LTM/MSP.
 */
#ifndef SMD_DB_CRC_H
#define SMD_DB_CRC_H

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint8_t crc_dvb_s2_table[256];
uint8_t crc8_dvb_s2(uint8_t crc, unsigned char a);
uint8_t crc8_dvb_s2_table(uint8_t crc, unsigned char a);

#ifdef __cplusplus
}
#endif

#endif /* SMD_DB_CRC_H */
