#pragma once

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/if_tun.h>
#include <linux/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/uio.h>
#include <cstring>

#include "TUN_constants.h"

struct TunSource
{
    int fd = -1;

    // Crea l'interfaccia e restituisce l'FD
    int init(int mtu, bool primary)
    {
        fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK);
        if (fd < 0)
            return -1;

        struct ifreq ifr{};
        ifr.ifr_flags = IFF_TUN | IFF_NO_PI; // TUN senza pacchetto info extra
        std::strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

        if (ioctl(fd, TUNSETIFF, &ifr) < 0)
            return -1;

        // Configurazione IP e MTU tramite socket temporaneo
        int sock = socket(AF_INET, SOCK_DGRAM, 0);

        // Imposta Indirizzo Locale
        struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_addr;
        addr->sin_family = AF_INET;
        inet_pton(AF_INET, primary ? addr_primary : addr_secondary, &addr->sin_addr);
        ioctl(sock, SIOCSIFADDR, &ifr);

        // Imposta Indirizzo Destinazione (P-t-P)
        inet_pton(AF_INET, primary ? addr_secondary : addr_primary, &addr->sin_addr);
        ioctl(sock, SIOCSIFDSTADDR, &ifr);

        // Imposta MTU
        ifr.ifr_mtu = mtu;
        ioctl(sock, SIOCSIFMTU, &ifr);

        // Reduce Queue length. Drop packets if too many
        ifr.ifr_qlen = 20; // Default è 1000. 20 è sufficiente per non bloccare TCP ma scartare eccessi.
        if (ioctl(sock, SIOCSIFTXQLEN, &ifr) < 0)
        {
            perror("Warning: Failed to set txqueuelen");
        }

        // Porta l'interfaccia UP
        ioctl(sock, SIOCGIFFLAGS, &ifr);
        ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);
        ioctl(sock, SIOCSIFFLAGS, &ifr);

        close(sock);
        return fd;
    }

    // Legge direttamente nella matrice del Processor usando iovec
    inline ssize_t read_to_buffer_iovec(struct iovec *iov, unsigned int count)
    {
        return readv(fd, iov, count);
    }

    inline ssize_t read_to_buffer(uint8_t *data, unsigned int length)
    {
        return read(fd, data, length);
    }

#include <errno.h> // Necessario per errno

    inline bool write_to_buffer(uint8_t *data, unsigned int length)
    {
        // Usa ssize_t per gestire il -1
        ssize_t written = write(get_fd(), data, length);

        if (written != (ssize_t)length)
        {
            if (written < 0)
            {
                // Stampa la vera causa dell'errore (es: "Message too long")
                perror("TUN Write Error");
            }
            else
            {
                printf("Partial write: %u vs %ld\n", length, written);
            }
            return false;
        }
        return true;
    }

#include <cstdio>
#include <cerrno>
#include <cstring>

    inline void write_to_buffer_iovec(struct iovec *iov, int max_iov, size_t msglen, unsigned int seglen)
    {
        unsigned int remainder = msglen % seglen;
        unsigned int segments = msglen / seglen;
        if (remainder != 0)
        {
            segments++;
            unsigned int slen = iov[segments - 1].iov_len;
            iov[segments - 1].iov_len = remainder;
            size_t written = writev(fd, iov, segments);
            iov[segments - 1].iov_len = slen;

            if (written != msglen)
            {
                printf("Read: %li written %li \n", msglen, written);
            }
        }
        else
        {
            writev(fd, iov, segments);
        }
    }

    // Restituisce l'FD per il poll() nel loop principale
    inline int get_fd() const
    {
        return fd;
    }

    void close_tun()
    {
        if (fd >= 0)
        {
            close(fd);
            fd = -1;
        }
    }
};