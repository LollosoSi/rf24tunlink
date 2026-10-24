#pragma once

// #define PACKET_LOGGER

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>

enum DIRECTION {
    TX,
    RX,
    NONE
};

const std::string directions[] = {"TX", "RX", "NONE"};

enum Packet_Types{
    DATA = 0,
    ACK = 1,
    NACK = 2,
    LAST = 3,
    INVALID_ACK = 4,
    INVALID_NACK = 5,
    FREE_ACK = 6,
    WTF = 7,
    RESEND_TIMEOUT = 8,
    RESEND_ASKED = 9,
    RESEND_AFTER_INVALID_ACK = 10,
    EXPIRED = 11,
    RECEPTION_RESET = 12
};

const std::string packet_types[] = {
    "DATA",
    "ACK",
    "NACK",
    "LAST",
    "INVALID_ACK",
    "INVALID_NACK",
    "FREE_ACK",
    "WTF",
    "RESEND_TIMEOUT",
    "RESEND_ASKED",
    "RESEND_AFTER_INVALID_ACK",
    "EXPIRED",
    "RECEPTION_RESET"
};

struct PacketLogger {
    std::ofstream log_file;

    void open(const std::string& filename) {
        log_file.open(filename, std::ios::out /* | std::ios::app*/);
        if (log_file.is_open()) {
            log_file << "Timestamp_us,Direction,ID,Type,Segment,Notes\n";
        }
    }

    uint64_t get_time_us() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    void write_log(DIRECTION dir, int id, Packet_Types type, int seg, const char* notes = "") {
        if (!log_file.is_open()) return;
        log_file << get_time_us() << "," << directions[dir] << "," << id << "," 
                 << packet_types[type] << "," << seg << "," << notes << "\n";
        log_file.flush();
    }

    void log_messaggio_completo(DIRECTION dir, int id, uint8_t crc, unsigned int len, unsigned int segments) {
        if (!log_file.is_open()) return;
        log_file << get_time_us() << "," << directions[dir] << "," << id << ",COMPLETE_MSG," << segments << ","
                 << "CRC:" << (int)crc << "|LEN:" << len << "\n";
        log_file.flush();
    }

    void close() {
        if (log_file.is_open()) log_file.close();
    }
};

static PacketLogger PL;
