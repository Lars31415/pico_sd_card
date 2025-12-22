#ifndef SMA_SD_CARD_H
#define SMA_SD_CARD_H

#include <hardware/spi.h>

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    const uint16_t sma_sd_block_size = 512;

    enum SMA_SD_SPI_ERROR
    {
        SMA_SD_OK = 0,
        SMA_SD_R1 = -1,     // Card rejected command
        SMA_SD_TOKEN = -2,  // Data start token (0xFE) never arrived
        SMA_SD_INCON = -3,  // Inconsistent data length
        SMA_SD_CRC = -4,    // CRC mismatch
        SMA_SD_RANGE = -5,  // Block address out of bounds
        SMA_SD_WRITE = -6,  // Card rejected data or write failed
        SMA_SD_BUSY = -7,   // Card stayed busy (DO low) too long
        SMA_SD_TIMEOUT = -8 // General communication timeout
    };

    typedef struct sma_sd_descriptor
    {
        uint8_t sdhc;
        uint8_t csd_structure; // 0 - 1
        uint32_t c_size;
        uint32_t block_count;
        uint32_t card_size; // bytes
    } sma_sd_descriptor_t;

    typedef struct sma_sd_sdi_config
    {
        spi_inst_t *spi;
        uint8_t rx_pin;
        uint8_t cs_pin;
        uint8_t clk_pin;
        uint8_t tx_pin;
        uint baud;
        sma_sd_descriptor_t desc;
    } sma_sd_config_t;

    bool sma_sd_generate_std_config(sma_sd_config_t *cfg);
    bool sma_sd_generate_config(const uint8_t rx_pin, const uint32_t baud, sma_sd_config_t *cfg);

    int sma_sd_init(sma_sd_config_t *cfg);
    int sma_sd_read_block(sma_sd_config_t *cfg, uint32_t block, uint8_t *buf, uint16_t *crc);
    int sma_sd_write_block(sma_sd_config_t *cfg, uint32_t block, uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif