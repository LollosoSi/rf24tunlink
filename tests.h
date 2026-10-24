#pragma once

#include <stdint.h>

#include <sys/uio.h>

#include <cmath>

#include "Modules/Processing/HARQ/CRC.h"

void test_crc4_8()
{
    unsigned int msglen = 36;
    unsigned int iovec_len_bytes = 5;
    uint8_t message[msglen];
    unsigned int iovec_len = ceil(msglen / (float)iovec_len_bytes);
    iovec data[iovec_len];
    for (unsigned int i = 0; i < msglen; i++)
    {
        message[i] = i % 128;
    }

    for (unsigned int i = 0; i < iovec_len; i++)
    {
        data[i].iov_base = message + (i * iovec_len_bytes);
        data[i].iov_len = iovec_len_bytes;
    }
    data[iovec_len - 1].iov_len = msglen % iovec_len_bytes;

    unsigned int iov_crc4 = gencrc4_iovec(data, iovec_len, msglen),
                 iov_crc8 = gencrc8_iovec(data, iovec_len, msglen),
                 crc4 = gencrc4(message, msglen),
                 crc8 = gencrc8(message, msglen);

    bool ver_4 = iov_crc4 == crc4;
    bool ver_8 = iov_crc8 == crc8;

    if (ver_4 && ver_8)
    {
        printf("CRC Test pass\n");
    }
    if (!ver_4)
        printf("CRC 4 Test Failed\n");
    if (!ver_8)
        printf("CRC 8 Test Failed\n");
}