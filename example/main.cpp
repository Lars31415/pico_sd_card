/*********************** !!! WARNING !!! **********************/
/* This example will overwrite any data on the sd-card.       */
/* Don't use with a card containing anything you want to keep */
/*********************** !!! WARNING !!! **********************/
#include "sma_sd_card.h"
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

    sma_sd_config_t cfg;
    int res = 0;
    std::cout << (res = sma_generate_std_config(&cfg)) << " " << std::endl;
    if (res == 0)
    {
        if (sma_sd_init(&cfg) >= 0)
        {
            uint32_t test_block_no{0};

            // Read block
            uint8_t buf[512];
            std::memset(buf, 0, sizeof(buf));
            uint16_t crc{0};
            sma_sd_read_block(&cfg, test_block_no, buf, &crc);
            print_buffer(buf, sizeof(buf));
            printf("\n");

            // Write block
            uint8_t wbuf[512];
            srand(std::time(nullptr));
            for (int i = 0; i < sizeof(wbuf); ++i)
                wbuf[i] = rand() % 0xFF;
            print_buffer(wbuf, sizeof(wbuf));
            sma_sd_write_block(&cfg, test_block_no, wbuf);

            // Read block
            std::memset(buf, 0, sizeof(buf));
            crc = 0;
            sma_sd_read_block(&cfg, test_block_no, buf, &crc);
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
