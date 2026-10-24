#pragma once

#include "HARQ_constants.h"

// #define fast_rs_with_crc

// #define fast_rs

#define safe_rs_crc

#ifdef fast_rs_with_crc

#include "CRC.h"

#include "ReedSolomon/RS-FEC.h"

#include <stdint.h>

const uint8_t RS_DATA_LEN = bytes_data;
const uint8_t RS_CRC_LEN = 1;
const uint8_t RS_ECC_LEN = bytes_ECC - RS_CRC_LEN;

static_assert(RS_ECC_LEN > 0);

RS::ReedSolomon<RS_DATA_LEN + RS_CRC_LEN, RS_ECC_LEN> rs;

uint8_t empty_data_array_for_comparison[RS_DATA_LEN] = {0};

/**
 * Encode in-place: l'input deve avere spazio per ecc_size alla fine
 * in_out deve essere lungo almeno (data_size + ecc_size)
 */
void rs_encode(uint8_t *in_out)
{
    // We will never send all zeroes
    assert(memcmp(empty_data_array_for_comparison, in_out, bytes_data));

    // EncodeBlock scrive l'ECC direttamente a partire dall'indirizzo fornito
    // Basandosi sulla struttura: [DATA (223 bytes)][ECC (32 bytes)]
    in_out[RS_DATA_LEN] = gencrc(in_out, RS_DATA_LEN); // CRC8 on the data bytes
    rs.EncodeBlock(in_out, in_out + RS_DATA_LEN + RS_CRC_LEN);
}

/**
 * Decode in-place: corregge direttamente i dati dentro in_out
 * @return true se successo
 */
bool rs_decode(uint8_t *in_out)
{
    // Scrap empty packets
    if (!memcmp(empty_data_array_for_comparison, in_out, RS_DATA_LEN))
    {
        DLOG("ReedSolomon Codec: Empty packet scrapped");
        return false;
    }
    // Decode corregge i dati e l'ECC direttamente nel buffer sorgente
    // in_out deve contenere DATA+ECC (totale RS_DATA_LEN + RS_ECC_LEN)
    int result = rs.Decode(in_out, in_out);

    if (result == 0)
        return (gencrc(in_out, RS_DATA_LEN) == in_out[RS_DATA_LEN]); // If errors are corrected, verify CRC8

    return false;
}

#endif

#ifdef fast_rs

#include "ReedSolomon/RS-FEC.h"

#include <stdint.h>

/**
 * @brief Scrambling/Descrambling simmetrico.
 * @param data Puntatore ai dati da processare.
 * @param len Lunghezza dei dati (escluso il buffer RS parity).
 * @param seed Valore iniziale per la sequenza (diverso da 0).
 */
void apply_scramble(uint8_t *data, size_t len, uint8_t seed = 0xAA)
{
    uint8_t lfsr = seed;
    for (size_t i = 0; i < len; i++)
    {
        data[i] ^= lfsr;
        // Semplice generatore congruenziale lineare per cambiare lfsr ad ogni byte
        lfsr = (uint8_t)((lfsr * 31) + 123);
    }
}

// Esempio con dimensioni fisse (necessario per questa classe template)
// Se le dimensioni variano, dovrai istanziare diversi oggetti RS
const uint8_t RS_DATA_LEN = bytes_data;
const uint8_t RS_ECC_LEN = bytes_ECC;

static RS::ReedSolomon<RS_DATA_LEN, RS_ECC_LEN> rs;

/**
 * Encode in-place: l'input deve avere spazio per ecc_size alla fine
 * in_out deve essere lungo almeno (data_size + ecc_size)
 */
static void rs_encode(uint8_t *in_out)
{
    apply_scramble(in_out, RS_DATA_LEN);
    // EncodeBlock scrive l'ECC direttamente a partire dall'indirizzo fornito
    // Basandosi sulla struttura: [DATA (223 bytes)][ECC (32 bytes)]
    rs.EncodeBlock(in_out, in_out + RS_DATA_LEN);
}

/**
 * Decode in-place: corregge direttamente i dati dentro in_out
 * @return 0 se successo, >0 se fallimento
 */
static bool rs_decode(uint8_t *in_out)
{
    static uint8_t empty_data_array_for_comparison[bytes_data] = {0};
    // Scrap empty packets
    if (!memcmp(empty_data_array_for_comparison, in_out, bytes_data))
    {
        return false;
    }
    // Decode corregge i dati e l'ECC direttamente nel buffer sorgente
    // in_out deve contenere DATA+ECC (totale RS_DATA_LEN + RS_ECC_LEN)
    int result = rs.Decode(in_out, in_out);
    apply_scramble(in_out, RS_DATA_LEN);

    return (result == 0);
}

#endif

#ifdef safe_rs_crc

#include "CRC.h"

#include <stdint.h>

const uint8_t RS_DATA_LEN = bytes_data;
const uint8_t RS_CRC_LEN = 1;
const uint8_t RS_ECC_LEN = bytes_ECC - RS_CRC_LEN;

uint8_t empty_data_array_for_comparison[RS_DATA_LEN] = {0};

#include "ReedSolomon/ReedSolomon.h"

static ReedSolomon rs(8);

#include <assert.h>

static void rs_encode(uint8_t *in_out)
{
    // We will never send all zeroes
    assert(memcmp(empty_data_array_for_comparison, in_out, bytes_data));

    // EncodeBlock scrive l'ECC direttamente a partire dall'indirizzo fornito
    // Basandosi sulla struttura: [DATA (223 bytes)][ECC (32 bytes)]
    in_out[RS_DATA_LEN] = gencrc(in_out, RS_DATA_LEN); // CRC8 on the data bytes
#if (RS_ECC_LEN > 0)
    
        // By default the library appends ECC bytes at the end, our array is actually one iovec which points: header, data, trailer, but the final layout will be trailer, header, data (yep, needs better naming)
        RS_WORD data_out[bytes_data + bytes_ECC];

        for (unsigned int i = 0; i < bytes_data + bytes_ECC; i++)
            data_out[i] = static_cast<RS_WORD>(in_out[i]);

        rs.encode(data_out, data_out, bytes_data + RS_CRC_LEN, bytes_ECC - RS_CRC_LEN);

        for (unsigned int i = 0; i < bytes_data + bytes_ECC; i++)
            in_out[i] = static_cast<uint8_t>(data_out[i]);
    
#endif
}

static bool rs_decode(uint8_t *in_out)
{

    // Scrap empty packets
    if (!memcmp(empty_data_array_for_comparison, in_out, RS_DATA_LEN))
    {
        DLOG("ReedSolomon Codec: Empty packet scrapped");
        return false;
    }

#if (RS_ECC_LEN > 0)
    int ecount = 0;

    RS_WORD data_out[bytes_data + bytes_ECC];

    for (unsigned int i = 0; i < bytes_data + bytes_ECC; i++)
        data_out[i] = static_cast<RS_WORD>(in_out[i]);

    if (!rs.decode(nullptr, data_out, data_out, bytes_data + RS_CRC_LEN, bytes_ECC - RS_CRC_LEN, nullptr, true, &ecount))
    {
        return false;
    }

    for (unsigned int i = 0; i < bytes_data + bytes_ECC; i++)
        in_out[i] = static_cast<uint8_t>(data_out[i]);

#endif

    return gencrc(in_out, RS_DATA_LEN) == in_out[RS_DATA_LEN];
}

#endif

#ifdef safe_rs

#include "ReedSolomon/ReedSolomon.h"

static ReedSolomon rs(8);

static void rs_encode(uint8_t *in_out)
{
    // By default the library appends ECC bytes at the end, our array is actually one iovec which points: header, data, trailer, but the final layout will be trailer, header, data (yep, needs better naming)
    RS_WORD data_out[bytes_data + bytes_ECC];

    for (unsigned int i = 0; i < bytes_data + bytes_ECC; i++)
        data_out[i] = static_cast<RS_WORD>(in_out[i]);

    rs.encode(data_out, data_out, bytes_data, bytes_ECC);

    for (unsigned int i = 0; i < bytes_data + bytes_ECC; i++)
        in_out[i] = static_cast<uint8_t>(data_out[i]);
}

static bool rs_decode(uint8_t *in_out)
{

    int ecount = 0;
    RS_WORD data_out[bytes_data + bytes_ECC];

    for (unsigned int i = 0; i < bytes_data + bytes_ECC; i++)
        data_out[i] = static_cast<RS_WORD>(in_out[i]);

    if (!rs.decode(nullptr, data_out, data_out, bytes_data, bytes_ECC, nullptr, true, &ecount))
    {
        return false;
    }

    for (unsigned int i = 0; i < bytes_data + bytes_ECC; i++)
        in_out[i] = static_cast<uint8_t>(data_out[i]);

    return true;
}

#endif