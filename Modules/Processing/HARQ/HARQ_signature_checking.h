#pragma once

#include "HARQ_constants.h"
#include "HARQ_data_structures.h"

struct Signature
{
    uint8_t id = max_id, segment;
    bool lp;
};

Signature crc_hash_signature[256];

void reset_duplicate_counting()
{
    for (unsigned int i = 0; i < sizeof(crc_hash_signature) / sizeof(Signature); i++)
        crc_hash_signature[i].id = max_id;
}

bool is_duplicate(segment &msg, bool ignore_lp = false)
{
    uint8_t all_crc = gencrc(msg.header, bytes_payload);
    uint8_t id, seg;
    bool lp;
    unpack(msg.header[0], id, seg, lp);

    if (ignore_lp && lp)
        return false;

    bool ver_id = crc_hash_signature[all_crc].id == id,
         ver_segment = crc_hash_signature[all_crc].segment == seg,
         ver_lp = crc_hash_signature[all_crc].lp == lp;

    if (ver_id && ver_segment && ver_lp)
    {
        DLOG("Duplicate packet detected, CRC: %d, ID: %d, SEG: %d, LP: %d", all_crc, id, seg, lp);
        return true;
    }
    else
    {
        crc_hash_signature[all_crc].id = id;
        crc_hash_signature[all_crc].segment = seg;
        crc_hash_signature[all_crc].lp = lp;
        return false;
    }
}