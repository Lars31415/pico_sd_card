/*********************** !!! WARNING !!! **********************/
/* This example will overwrite any data on the sd-card.       */
/* Don't use with a card containing anything you want to keep */
/*********************** !!! WARNING !!! **********************/
#include "sd_card.h"
#include "crc7.h"
#include "crc16.h"

#include <pico/binary_info.h>
#include <hardware/spi.h>
#include <pico/stdio.h>
#include <pico/stdlib.h>
#include <pico/bootrom.h>
#include <ctime>
#include <cstring>
#include <array>

#include <iostream>

// const uint8_t ff{0xFF};
// typedef std::array<uint8_t, 6> Cmd;
// bool sdhc{false};

extern "C"
{
    void print_buffer(const uint8_t *bf, uint16_t sz)
    {
        if (sz == 0)
            printf("*none*");
        else
        {
            for (int i = 0; i < sz; ++i)
                printf("%02X ", bf[i]);
        }
        printf("\n");
    }
}
// void encode_addr(uint32_t addr, uint8_t *p)
// {
//     if (!sdhc)
//         addr *= 512;
//     *(p) = uint8_t(addr >> 24); // MSB
//     *(p + 1) = uint8_t(addr >> 16);
//     *(p + 2) = uint8_t(addr >> 8);
//     *(p + 3) = uint8_t(addr);
// }

// inline uint8_t sd_wait_r1(void)
// {
//     uint8_t r = 0xFF;
//     uint8_t ff = 0xFF;

//     for (int i = 0; i < 1000; i++)
//     {
//         spi_write_read_blocking(spi0, &ff, &r, 1);
//         if ((r & 0x80) == 0)
//             return r;
//     }
//     printf("r1 timeout, last: 0x%02X\n", r);
//     return 0xFF;
// }

// inline uint8_t sd_wait_token()
// {
//     uint8_t r;
//     for (int i = 0; i < 100000; i++)
//     {
//         spi_write_read_blocking(spi0, &ff, &r, 1);
//         if (r == 0xFE) // bit7 cleared
//             return r;
//     }
//     printf("token error: 0x%02X\n", r);
//     return ff; // timeout
// }

// inline Cmd &sd_encode_cmd(Cmd &cmd)
// {
//     cmd[0] |= 0x40;
//     if ((cmd[0] == 0x40) || (cmd[0] == 0x48))
//         cmd[5] = mmc_sdc_crc7(cmd.data(), 5);
//     else
//         cmd[5] = 0xFF;
//     return cmd;
// }

// int sd_cmd(Cmd cmd, uint8_t *rep, uint16_t len, uint16_t *bytes)
// {
//     cmd = sd_encode_cmd(cmd);

//     // Send commend
//     gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 0);
//     spi_write_blocking(spi0, cmd.data(), 6);
//     // flush
//     spi_write_blocking(spi0, &ff, 1);

//     // Wait for response
//     uint8_t r1{sd_wait_r1()};

//     if (rep == nullptr)
//     {
//         gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
//         spi_write_blocking(spi0, &ff, 1);
//         return r1;
//     }

//     int rl = spi_read_blocking(spi0, ff, rep, len);
//     gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
//     spi_write_blocking(spi0, &ff, 1);

//     if (bytes != nullptr)
//         *bytes = rl;

//     return r1;
// }

// int sd_read_block(uint16_t block, uint8_t *buf, uint16_t *crc)
// {
//     uint8_t token;
//     uint8_t r1;

//     Cmd cmd = {0x11, 0, 0, 0, 0, 0};
//     encode_addr(block, cmd.data() + 1);
//     cmd = sd_encode_cmd(cmd);

//     gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 0);

//     // Send command
//     spi_write_blocking(spi0, cmd.data(), 6);

//     // Clock until R1 appears
//     r1 = sd_wait_r1();
//     if (r1 != 0x00)
//     {
//         printf("CMD17 failed, R1=0x%02X\n", r1);
//         gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
//         spi_write_blocking(spi0, &ff, 1);
//         return 0;
//     }

//     // Wait for data token
//     for (int i = 0; i < 100000; i++)
//     {
//         spi_write_read_blocking(spi0, &ff, &token, 1);
//         if (token == 0xFE)
//             break;
//     }

//     if (token != 0xFE)
//     {
//         printf("Data token timeout: 0x%02X\n", token);
//         gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
//         spi_write_blocking(spi0, &ff, 1);
//         return 0;
//     }

//     // Read data
//     int rl = spi_read_blocking(spi0, ff, buf, 512);
//     spi_read_blocking(spi0, ff, (uint8_t *)crc, 2);
//     print_buffer(buf, rl);
//     printf("crc: 0x%04X", *crc);
//     if (*crc == change_order(crc16(buf, rl, 0)))
//         printf(" valid\n");
//     else
//         printf(" INVALID\n");
//     gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
//     spi_write_blocking(spi0, &ff, 1);

//     return rl;
// }

// int sd_write_block(uint16_t block, uint8_t *buf)
// {
//     uint8_t token{0xFE};
//     uint8_t r1;

//     Cmd cmd = {0x18, 0, 0, 0, 0, 0};
//     encode_addr(block, cmd.data() + 1);
//     cmd = sd_encode_cmd(cmd);

//     gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 0);

//     // Send command
//     spi_write_blocking(spi0, cmd.data(), 6);

//     // Clock until R1 appears
//     r1 = sd_wait_r1();
//     if (r1 != 0x00)
//     {
//         printf("CMD24 failed, R1=0x%02X\n", r1);
//         gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
//         spi_write_blocking(spi0, &ff, 1);
//         return 0;
//     }

//     spi_write_blocking(spi0, &token, 1);

//     spi_write_blocking(spi0, buf, 512);

//     uint16_t crc = change_order(crc16(buf, 512, 0));
//     spi_write_blocking(spi0, (uint8_t *)&crc, 2);

//     printf("crc: 0x%04X\n\n", crc);

//     uint8_t resp;
//     spi_write_read_blocking(spi0, &ff, &resp, 1);

//     if ((resp & 0x1F) != 0x05)
//     {
//         printf("Write rejected: 0x%02X\n", resp);
//         gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
//         spi_write_blocking(spi0, &ff, 1);
//         return false;
//     }

//     uint8_t busy;
//     do
//     {
//         spi_write_read_blocking(spi0, &ff, &busy, 1);
//     } while (busy != 0xFF);

//     gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
//     spi_write_blocking(spi0, &ff, 1);

//     return r1;
// }

// bool sd_read_csd(uint8_t *csd)
// {
//     Cmd cmd = {0x09, 0, 0, 0, 0, 0};
//     cmd = sd_encode_cmd(cmd);

//     gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 0);
//     spi_write_blocking(spi0, cmd.data(), 6);

//     if (sd_wait_r1() != 0x00)
//     {
//         gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
//         return false;
//     }

//     if (sd_wait_token() != 0xFE)
//     {
//         gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
//         return false;
//     }

//     spi_read_blocking(spi0, ff, csd, 16);
//     uint16_t crc;
//     spi_read_blocking(spi0, ff, (uint8_t *)&crc, 2);

//     gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
//     spi_write_blocking(spi0, &ff, 1);
//     return true;
// }

int main(int, char **)
{
    //// Startup code for USB-console start
    bool flag = false;
    //// Give Windows a chance to forget usb-drive and discover usb-serial device
    sleep_ms(1000);
    stdio_init_all();
    for (int i = 0; (i < 50) && (!flag); ++i)
    {
        flag = stdio_usb_connected();
        sleep_ms(100);
    }
    std::cout << "Startup!" << std::endl;
    std::cout << "sd_test" << std::endl;
    std::cout << "C:/github/RaspberryPi/Pico/sd_test" << std::endl;
    std::cout << "main.cpp" << std::endl;
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << std::endl;
    std::cout << "Ok, let's go!" << std::endl;
    //// Startup code for USB-console end

    printf("\n");

    sd_config_t cfg;
    int res = 0;
    std::cout << (res = generate_std_config(&cfg)) << " " << std::endl;
    if (res == 0)
    {
        if (sd_init(&cfg) >= 0)
        {
            uint32_t test_block_no{0};

            // Read block
            uint8_t buf[512];
            std::memset(buf, 0, sizeof(buf));
            uint16_t crc{0};
            sd_read_block(&cfg, test_block_no, buf, &crc);
            print_buffer(buf, sizeof(buf));
            printf("\n");

            // Write block
            uint8_t wbuf[512];
            srand(std::time(nullptr));
            for (int i = 0; i < sizeof(wbuf); ++i)
                wbuf[i] = rand() % 0xFF;
            print_buffer(wbuf, sizeof(wbuf));
            sd_write_block(&cfg, test_block_no, wbuf);

            // Read block
            std::memset(buf, 0, sizeof(buf));
            crc = 0;
            sd_read_block(&cfg, test_block_no, buf, &crc);
            print_buffer(buf, sizeof(buf));
            printf("\n");

            if (std::memcmp(buf, wbuf, sizeof(buf)) == 0)
                printf("write-read block success!\n");
            else
                printf("write-read block failed\n");
        }
    }

    // End code for USB-console start
    std::cout << std::endl
              << "End program!" << std::endl;
    std::cout << std::boolalpha << flag << std::endl;
    stdio_flush();
    if (flag)
    {
        std::cout << "Send 0x03 char..." << std::endl;
        while (getchar() != 0x03)
            ;
        std::cout << "Goodbye :)" << std::endl;
        // std::cout << "\033[2J" << std::endl;
        std::cout << "\033c";
        std::cout << "Boot ROM!" << std::endl;
    }

    stdio_flush();
    sleep_ms(500);
    reset_usb_boot(0, 0);
    //// End code for USB-console end
}
