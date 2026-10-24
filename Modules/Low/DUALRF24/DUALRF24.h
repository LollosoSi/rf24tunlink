#pragma once

#include "DUALRF24_constants.h"

#include "../../Processing/HARQ/HARQ.h"

#include <RF24/RF24.h>

extern void radio_read_IRQ();

#include <fcntl.h>  // Per open() e O_RDONLY
#include <unistd.h> // Per lseek(), read() e close()
#include <cstdio>   // Per snprintf()

RF24 radio0;
RF24 radio1;
bool radio_ready_to_use = false;

bool role = 1;

inline bool check_radio_status(RF24 &radio)
{
    bool c1 = radio.failureDetected;
    bool c2 = radio.getDataRate() != radio_speed;
    bool c3 = !radio.isChipConnected();
    if (c1 || c2 || c3)
    {
        printf("Radio %d has failure. Failure Detected: %d, Invalid DataRate %d, Is Chip Connected %d\n", &radio == &radio0 ? 0 : 1, c1, c2, c3);
        radio.failureDetected = 0;
        return (true);
    }
    else
    {
        return (false);
    }
}

inline void ResetRadio0(bool print_info = false)
{

    if (!radio_ready_to_use)
    {
        printf("Reset radio 0 called without radio 0\n");
        throw std::invalid_argument("Invalid radio 0");
    }

    uint8_t t = 1;
    while (!radio0.begin(radio_0_ce,
                         radio_0_csn))
    {
        printf("Radio 0 is not responsive (CE: %i, CSN: %i)\n",
               radio_0_ce, radio_0_csn);
        // usleep(10000);
        // delay(2);
        if (!t++)
        {
            // handleRadioUnresponsiveTimeout();
        }
    }

    radio0.flush_rx();
    radio0.flush_tx();

    radio0.setAddressWidth(address_bytes);

    radio0.setPayloadSize(payload_size);

    radio0.setPALevel(radio_power);
    radio0.setDataRate((rf24_datarate_e)radio_speed);
    radio0.setRetries(radio_delay,
                      radio_retries);

    radio0.setAutoAck(auto_ack);

    if (dynamic_payloads)
        radio0.enableDynamicPayloads();
    else
        radio0.disableDynamicPayloads();

    if (ack_payloads)
        radio0.enableAckPayload();
    else
        radio0.disableAckPayload();

    radio0.setCRCLength((rf24_crclength_e)CRC_LENGTH);

    if (role)
    {
        const uint8_t *ln = address_0;
        radio0.setChannel(channel_0);
        radio0.openReadingPipe(1, ln);
        printf("RADIO0 -> ROLE: %d \t channel: %d\n",
               role,
               channel_0);
    }
    else
    {
        const uint8_t *ln = address_1;
        radio0.setChannel(channel_1);
        radio0.openReadingPipe(1, ln);
        printf("RADIO0 -> ROLE: %d \t channel: %d\n",
               role,
               channel_1);
    }

    // Configure radio0 to interrupt for read events
    // Deprecated, now replaced with setStatusFlags
    // radio0.maskIRQ(true,true,false);
    radio0.setStatusFlags(RF24_RX_DR);

    radio0.startListening();
    if (print_info)
    {
        radio0.printPrettyDetails();
    }

    attachInterrupt(radio_0_irq, INT_EDGE_FALLING, &radio_read_IRQ);
}

#include <atomic> // Meglio di volatile su Linux/C++ moderno
#include <thread> // Per yield()

// Usa atomic per thread-safety tra IRQ e Main loop
std::atomic<bool> wait_tx(false);

void tx_IRQ()
{
    radio1.clearStatusFlags(RF24_IRQ_ALL); // We don't care about the value, this radio is tx only
    wait_tx = false;
}

inline void ResetRadio1(bool print_info = false)
{

    if (!radio_ready_to_use)
    {
        printf("Reset radio 1 called without radio 1\n");
        throw std::invalid_argument("Invalid radio 1");
    }

    uint8_t t = 1;
    while (!radio1.begin(radio_1_ce,
                         radio_1_csn))
    {
        printf("Radio 1 is not responsive (CE: %i, CSN: %i)\n",
               radio_1_ce, radio_1_csn);
        // usleep(10000);
        // delay(2);
        if (!t++)
        {
            // handleRadioUnresponsiveTimeout();
        }
    }

    radio1.flush_rx();
    radio1.flush_tx();

    radio1.setAddressWidth(address_bytes);

    radio1.setPayloadSize(payload_size);

    radio1.setPALevel(radio_power);
    radio1.setDataRate((rf24_datarate_e)radio_speed);
    radio1.setRetries(radio_delay,
                      radio_retries);

    radio1.setAutoAck(auto_ack);

    if (dynamic_payloads)
        radio1.enableDynamicPayloads();
    else
        radio1.disableDynamicPayloads();

    if (ack_payloads)
        radio1.enableAckPayload();
    else
        radio1.disableAckPayload();

    radio1.setCRCLength((rf24_crclength_e)CRC_LENGTH);

    if (role)
    {
        const uint8_t *ln = address_1;
        radio1.setChannel(channel_1);
        radio1.stopListening(ln);
        printf("RADIO1 -> ROLE: %d \t channel: %d. NOTE Reading pipe is disabled\n",
               role,
               channel_1);
    }
    else
    {
        const uint8_t *ln = address_0;
        radio1.setChannel(channel_0);
        radio1.stopListening(ln);
        printf("RADIO1 -> ROLE: %d \t channel: %d. NOTE Reading pipe is disabled\n",
               role,
               channel_0);
    }

    // Configure radio1 to interrupt for write events
    // radio1.maskIRQ(false, false, true);
    radio1.setStatusFlags(RF24_TX_DS | RF24_TX_DF);

    if (print_info)
        radio1.printPrettyDetails();

    // attachInterrupt(radio_1_irq, INT_EDGE_FALLING, &tx_IRQ);
}

inline void send_tx()
{
    if (!radio1.txStandBy())
    {
        radio1.flush_tx();
    }
}

#undef INPUT
#undef OUTPUT

#include "GPIOINTERRUPTHANDLER.h"

struct DUALRF24
{
    RadioHardware::GpioHandler gh;

    void init()
    {

        radio0 = RF24(radio_0_ce,
                      radio_0_csn, spi_speed);

        // if (!radio1)
        radio1 = RF24(radio_1_ce,
                      radio_1_csn, spi_speed);
        radio_ready_to_use = true;

        // if(!use_IRQ_RF24)
        // gh.setup(radio_0_irq);

        ResetRadio0();
        ResetRadio1();
    }

    // Read sequential bytes, return the number of packets read
    // This function assumes out_buffer is contiguous in memory
    int read_IRQ(uint8_t *out_buffer, int max_reads = 3)
    {
        // uint8_t flags = radio0.clearStatusFlags(RF24_IRQ_ALL);
        radio0.clearStatusFlags(RF24_IRQ_ALL); // We don't care about the value, this radio is read only
        int n = 0;
        while (radio0.available() && n < max_reads)
        {
            radio0.read(out_buffer + (n * payload_size), payload_size);
            if (rs_decode(out_buffer + (n * payload_size)))
            {
                n++;
            }
            else
            {
                DLOG("Radio packet scrapped while reading.");
            }
        }

        if (check_radio_status(radio0))
            ResetRadio0(false);

        return n;
    }

    // This function assumes out_buffer is contiguous in memory
    void write(uint8_t *out_buffer, int sequential_payloads)
    {
        if (check_radio_status(radio1))
            ResetRadio1(false);

        for (int i = 0; i < sequential_payloads; i++)
        {
            if (!radio1.writeFast(out_buffer + (payload_size * i), payload_size, !auto_ack))
                send_tx();
            // if (i % 3 == 0)
            //     send_tx();
        }

        if ((!radio1.isFifo(true)) != RF24_FIFO_EMPTY)
            send_tx();
    }

    // This function assumes out_buffer is contiguous in memory
    void radio_write(uint8_t *out_buffer, unsigned int sequential_payloads, unsigned int bytes_per_payload)
    {
        for (unsigned int i = 0; i < sequential_payloads; i++)
        {
            if (!radio1.writeFast(out_buffer + (bytes_per_payload * i), bytes_per_payload, !auto_ack))
                send_tx();
            if (i % 3 == 0 && i > 0)
                send_tx();
        }

        if ((!radio1.isFifo(true)) != RF24_FIFO_EMPTY)
            send_tx();

        if (check_radio_status(radio1))
            ResetRadio1(false);
    }

    // Ritorna il FD per il poll
    int get_fd() const { return gh.fd_irq; }

    // Gestore della lettura chiamato dal loop
    int handle_read(uint8_t *out_buffer, int max_reads = 3)
    {
        gh.clear();                             // Prima svuota il FD
        return read_IRQ(out_buffer, max_reads); // Poi svuota la FIFO della radio
    }
};