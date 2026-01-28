#ifndef HLY_SD_CARD_H
#define HLY_SD_CARD_H

#include <hardware/spi.h>

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    extern const uint16_t hly_sd_block_size;

    enum HLY_SD_SPI_ERROR
    {
        HLY_SD_OK = 0,
        HLY_SD_R1 = -1,     // Card rejected command
        HLY_SD_TOKEN = -2,  // Data start token (0xFE) never arrived
        HLY_SD_INCON = -3,  // Inconsistent data length
        HLY_SD_CRC = -4,    // CRC mismatch
        HLY_SD_RANGE = -5,  // Block address out of bounds
        HLY_SD_WRITE = -6,  // Card rejected data or write failed
        HLY_SD_BUSY = -7,   // Card stayed busy (DO low) too long
        HLY_SD_TIMEOUT = -8 // General communication timeout
    };

    typedef struct hly_sd_descriptor
    {
        uint8_t sdhc;
        uint8_t csd_structure; // 0 - 1
        uint32_t c_size;
        uint32_t block_count;
        uint32_t card_size; // bytes
    } hly_sd_descriptor_t;

    typedef struct hly_sd_sdi_config
    {
        spi_inst_t *spi;
        uint8_t rx_pin;
        uint8_t cs_pin;
        uint8_t clk_pin;
        uint8_t tx_pin;
        uint baud;
        hly_sd_descriptor_t desc;
    } hly_sd_config_t;

    bool hly_sd_generate_std_config(hly_sd_config_t *cfg);
    bool hly_sd_generate_config(const uint8_t rx_pin, const uint32_t baud, hly_sd_config_t *cfg);

    int hly_sd_init(hly_sd_config_t *cfg);
    int hly_sd_read_block(const hly_sd_config_t *cfg, uint32_t block, uint8_t *buf, uint16_t *crc);
    int hly_sd_write_block(const hly_sd_config_t *cfg, uint32_t block, uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif