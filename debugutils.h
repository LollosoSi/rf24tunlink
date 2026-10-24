#pragma once

#include <iostream>
#include <sys/uio.h>
#include <stdint.h>

#include <cmath>

#include <cstring>

template <typename T>
void print_array(T *array, int length, int splitby)
{
    for (int i = 0; i < length; i++)
    {
        if ((i % splitby) == 0 && i >= splitby)
            printf("\n");
        printf("%02x ", array[i]);
    }
    printf("\n");
}

void print_iovec_as_array(struct iovec *iov, int iovcnt, int total_to_print, int splitby)
{
    int remaining = total_to_print;
    for (int i = 0; i < iovcnt && remaining > 0; i++)
    {
        uint8_t *ptr = (uint8_t *)iov[i].iov_base;

        int to_print = std::min((int)iov[i].iov_len, remaining);
        for (int j = 0; j < to_print; j++)
        {
            if ((i % splitby) == 0 && i >= splitby)
            {
                printf("\n");
            }
            printf("%02x ", ptr[j]);
        }
        remaining -= to_print;
    }
    printf("\n");
}

void print_iovec(struct iovec *iov, int iovcnt)
{
    for (int i = 0; i < iovcnt; i++)
    {
        printf("Segmento %d (len: %zu): ", i, iov[i].iov_len);
        uint8_t *ptr = (uint8_t *)iov[i].iov_base;
        for (size_t j = 0; j < iov[i].iov_len; j++)
        {
            printf("%02d ", ptr[j]);
        }
        printf("\n");
    }
}

void print_iovec(struct iovec *iov, int iovcnt, int total_to_print)
{
    int remaining = total_to_print;
    for (int i = 0; i < iovcnt && remaining > 0; i++)
    {
        printf("Seg %d [len %zu]: ", i, iov[i].iov_len);
        uint8_t *ptr = (uint8_t *)iov[i].iov_base;

        int to_print = std::min((int)iov[i].iov_len, remaining);
        for (int j = 0; j < to_print; j++)
        {
            printf("%02x ", ptr[j]); // Hex è meglio per i pacchetti
        }
        printf("\n");
        remaining -= to_print;
    }
    printf("\n");
}

void flatten_iovec_safe(uint8_t *dest, size_t dest_size, const struct iovec *iov, int iovcnt, size_t n)
{
    size_t copied = 0;
    for (int i = 0; i < iovcnt && copied < n; ++i)
    {
        // Calcola quanti byte restano da copiare per questo segmento
        size_t to_copy = std::min(n - copied, (size_t)iov[i].iov_len);

        // Protezione overflow buffer destinazione
        if (copied + to_copy > dest_size)
        {
            to_copy = dest_size - copied;
        }

        if (iov[i].iov_base != nullptr && to_copy > 0)
        {
            std::memcpy(dest + copied, iov[i].iov_base, to_copy);
        }

        copied += to_copy;
        if (copied >= dest_size)
            break;
    }
}