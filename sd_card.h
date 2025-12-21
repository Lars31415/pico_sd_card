#ifndef SD_CARD_H
#define SD_CARD_H

#include <hardware/spi.h>

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    const uint16_t block_size = 512;

    enum SD_SPI_ERROR
    {
        E_OK = 0,
        E_R1 = -1,     // Card rejected command
        E_TOKEN = -2,  // Data start token (0xFE) never arrived
        E_INCON = -3,  // Inconsistent data length
        E_CRC = -4,    // CRC mismatch
        E_RANGE = -5,  // Block address out of bounds
        E_WRITE = -6,  // Card rejected data or write failed
        E_BUSY = -7,   // Card stayed busy (DO low) too long
        E_TIMEOUT = -8 // General communication timeout
    };

    typedef struct sd_descriptor
    {
        uint8_t sdhc;
        uint8_t csd_structure; // 0 - 1
        uint32_t c_size;
        uint32_t block_count;
        uint32_t card_size; // bytes
    } sd_descriptor_t;

    typedef struct sd_config
    {
        spi_inst_t *spi;
        uint8_t rx_pin;
        uint8_t cs_pin;
        uint8_t clk_pin;
        uint8_t tx_pin;
        uint baud;
        sd_descriptor_t desc;
    } sd_config_t;

    int generate_std_config(sd_config_t *cfg);
    int generate_config(const uint8_t rx_pin, const uint32_t baud, sd_config_t *cfg);

    int sd_init(sd_config_t *cfg);
    int sd_read_block(sd_config_t *cfg, uint32_t block, uint8_t *buf, uint16_t *crc);
    int sd_write_block(sd_config_t *cfg, uint32_t block, uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif