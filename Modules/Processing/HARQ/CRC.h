/*
 * crc.h
 *
 *  Created on: 28 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../../../debugutils.h"

#include <stdint.h>
#include <sys/uio.h>

//#define CRC_TABLE

#ifdef CRC_TABLE

#include <cstdint>
#include <cstddef>
#include <sys/uio.h>
#include <array>

// ============================================================================
// 1. GENERAZIONE TABELLE (Compile-Time / Zero-Overhead)
// ============================================================================

// Polinomio CRC8 standard (0x31) -> x^8 + x^5 + x^4 + 1
constexpr uint8_t CRC8_POLY = 0x31;
constexpr std::array<uint8_t, 256> generate_crc8_table() {
    std::array<uint8_t, 256> table{};
    for (int i = 0; i < 256; i++) {
        uint8_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) crc = static_cast<uint8_t>((crc << 1) ^ CRC8_POLY);
            else crc <<= 1;
        }
        table[i] = crc;
    }
    return table;
}

// Polinomio CRC4 ITU (0x13) -> x^4 + x + 1
constexpr uint8_t CRC4_POLY = 0x13;
constexpr std::array<uint8_t, 256> generate_crc4_table() {
    std::array<uint8_t, 256> table{};
    for (int i = 0; i < 256; i++) {
        uint8_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) crc = static_cast<uint8_t>((crc << 1) ^ CRC4_POLY);
            else crc <<= 1;
        }
        table[i] = crc & 0x0F; // Maschera fondamentale per CRC4
    }
    return table;
}

// Tabelle allocate nella sezione .rodata (Read Only)
static constexpr auto crc8_table = generate_crc8_table();
static constexpr auto crc4_table = generate_crc4_table();

// ============================================================================
// 2. IMPLEMENTAZIONI CRC8
// ============================================================================

// Versione standard (buffer lineare)
inline uint8_t gencrc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xff; // Init 0xFF come da tuo standard
    while (len--)
    {
        crc = crc8_table[crc ^ *data++];
    }
    return crc;
}

// Versione IOVEC (Scatter/Gather) ottimizzata
#pragma GCC optimize("unroll-loops")
inline uint8_t gencrc8_iovec(const iovec *data, size_t iovec_num, unsigned int total_bytes)
{
    uint8_t crc = 0xff;
    unsigned int count = 0;

    for (size_t iov = 0; iov < iovec_num; iov++)
    {
        uint8_t *base = static_cast<uint8_t *>(data[iov].iov_base);
        size_t len = data[iov].iov_len;
        
        for (size_t i = 0; i < len; i++)
        {
            if (count >= total_bytes) return crc;
            
            crc = crc8_table[crc ^ base[i]];
            count++;
        }
    }
    return crc;
}

// ============================================================================
// 3. IMPLEMENTAZIONI CRC4
// ============================================================================

// Versione standard (buffer lineare)
inline uint8_t gencrc4(const uint8_t *data, size_t len)
{
    uint8_t crc = 0; // Init 0 per CRC4
    while (len--)
    {
        // Shift << 4 per allineare il CRC4 (4 bit) con i dati (8 bit)
        crc = crc4_table[(crc << 4) ^ *data++];
    }
    return crc & 0x0F;
}

// Versione IOVEC (Scatter/Gather) ottimizzata
#pragma GCC optimize("unroll-loops")
inline uint8_t gencrc4_iovec(const iovec *data, size_t iovec_num, unsigned int total_bytes)
{
    uint8_t crc = 0;
    unsigned int count = 0;

    for (size_t iov = 0; iov < iovec_num; iov++)
    {
        uint8_t *base = static_cast<uint8_t *>(data[iov].iov_base);
        size_t len = data[iov].iov_len;

        for (size_t i = 0; i < len; i++)
        {
            if (count >= total_bytes) return (crc & 0x0F);

            crc = crc4_table[(crc << 4) ^ base[i]];
            count++;
        }
    }
    return crc & 0x0F;
}

#else

uint8_t gencrc(uint8_t *data, size_t len)
{
    uint8_t crc = 0xff;
    size_t i, j;
    for (i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (j = 0; j < 8; j++)
        {
            if ((crc & 0x80) != 0)
                crc = static_cast<uint8_t>(((crc << 1) ^ 0x31));
            else
                crc <<= 1;
        }
    }
    return (crc);
}

#pragma GCC optimize("unroll-loops")
uint8_t gencrc_iovec(iovec *data, size_t iovec_num, unsigned int bytes)
{
    uint8_t crc = 0xff;
    unsigned int count = 0;

    for (size_t iov = 0; iov < iovec_num; iov++)
    {
        uint8_t *base = static_cast<uint8_t *>(data[iov].iov_base);
        for (size_t i = 0; i < data[iov].iov_len; i++)
        {

            if (count >= bytes)
                return crc; // Controllo byte prima dell'elaborazione

            crc ^= base[i];
            for (size_t j = 0; j < 8; j++)
            {
                if ((crc & 0x80) != 0)
                    crc = (uint8_t)((crc << 1) ^ 0x31);
                else
                    crc <<= 1;
            }
            count++; // Incrementa dopo aver finito gli 8 bit del byte
        }
    }
    return crc;
}

#endif