/*
 * buffer.h
 *
 *  Created on: 13 maj 2013
 *      Author: benjamin
 */

#ifndef BUFFER_H_
#define BUFFER_H_

#include <stdint.h>

namespace utility {

void buffer_append_int32(uint8_t* buffer, int32_t number, int32_t *index);
void buffer_append_uint32(uint8_t* buffer, uint32_t number, int32_t *index);
void buffer_append_int16(uint8_t* buffer, int16_t number, int32_t *index);
void buffer_append_uint16(uint8_t* buffer, uint16_t number, int32_t *index);
int16_t buffer_get_int16(const uint8_t *buffer, int32_t *index);
uint16_t buffer_get_uint16(const uint8_t *buffer, int32_t *index);
int32_t buffer_get_int32(const uint8_t *buffer, int32_t *index);
uint32_t buffer_get_uint32(const uint8_t *buffer, int32_t *index);

}

static void save_u16(uint8_t* buf, uint16_t data) {
    buf[0] = data >> 8;
    buf[1] = data & 0xFF;
}

static void save_u32(uint8_t* buf, uint32_t data) {
    buf[0] = (data >> 24) & 0xFF;
    buf[1] = (data >> 16) & 0xFF;
    buf[2] = (data >> 8) & 0xFF;
    buf[3] = data & 0xFF;
}

static void save_16(uint8_t* buf, int16_t data) {
    buf[0] = data < 0 ? 1 : 0;
    save_u16(buf + 1, data < 0 ? -data : data);
}

static void save_32(uint8_t* buf, int32_t data) {
    buf[0] = data < 0 ? 1 : 0;
    save_u32(buf + 1, data < 0 ? -data : data);
}

static int16_t read_16(uint8_t* buf) {
    int16_t data = (static_cast<int16_t>(buf[1])<<8) + buf[2];
    if (buf[0] == 1) {
        data = -data;
    }
    return data;
}

static int32_t read_32(uint8_t* buf) {
    int32_t data = (static_cast<uint32_t>(buf[1])<<24) + (static_cast<uint32_t>(buf[2])<<16) + (static_cast<uint32_t>(buf[3])<<8) + buf[4];
    if (buf[0] == 1) {
        data = -data;
    }
    return data;
}

static int16_t read_u16(uint8_t* buf) {
    return (static_cast<int16_t>(buf[0])<<8) + buf[1];
}

static int32_t read_u32(uint8_t* buf) {
    return (static_cast<int32_t>(buf[0])<<24) + (static_cast<int32_t>(buf[1])<<16) + (static_cast<int32_t>(buf[2])<<8) + buf[3];
}


#endif /* BUFFER_H_ */
