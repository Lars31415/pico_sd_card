/**
 * @file hly_sd_card.h
 * @brief SPI-based SD card driver for Raspberry Pi Pico (RP2040).
 * * This library provides basic block-level read/write access to SD and SDHC cards
 * using the SPI protocol.
 */

#ifndef HLY_SD_CARD_H
#define HLY_SD_CARD_H

#include <hardware/spi.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Standard SD card block size in bytes. */
    extern const uint16_t hly_sd_block_size;

    /**
     * @brief Error codes for SD card operations.
     */
    enum HLY_SD_SPI_ERROR
    {
        HLY_SD_OK = 0,      /**< Operation successful */
        HLY_SD_R1 = -1,     /**< Card rejected command (R1 response != 0) */
        HLY_SD_TOKEN = -2,  /**< Data start token (0xFE) timed out */
        HLY_SD_INCON = -3,  /**< Inconsistent data length received */
        HLY_SD_CRC = -4,    /**< CRC mismatch detected */
        HLY_SD_RANGE = -5,  /**< Block address out of bounds */
        HLY_SD_WRITE = -6,  /**< Card rejected data or write failed */
        HLY_SD_BUSY = -7,   /**< Card stayed busy (DO low) too long */
        HLY_SD_TIMEOUT = -8 /**< General communication timeout */
    };

    /**
     * @brief Structure to hold SD card physical characteristics.
     */
    typedef struct hly_sd_descriptor
    {
        uint8_t sdhc;          /**< 1 if SDHC/SDXC (block addressing), 0 if Standard (byte addressing) */
        uint8_t csd_structure; /**< CSD Register version */
        uint32_t c_size;       /**< Device size value from CSD */
        uint32_t block_count;  /**< Total number of 512-byte blocks */
        uint32_t card_size;    /**< Total capacity in bytes */
    } hly_sd_descriptor_t;

    /**
     * @brief Configuration and state handle for the SD card instance.
     */
    typedef struct hly_sd_sdi_config
    {
        spi_inst_t *spi;          /**< Pointer to SPI instance (spi0 or spi1) */
        uint8_t rx_pin;           /**< MISO pin */
        uint8_t cs_pin;           /**< Chip Select pin */
        uint8_t clk_pin;          /**< Serial Clock pin */
        uint8_t tx_pin;           /**< MOSI pin */
        uint baud;                /**< Target baudrate for data transfer */
        hly_sd_descriptor_t desc; /**< Populated card descriptor after init */
    } hly_sd_config_t;

    /**
     * @brief Generates a default configuration using PICO_DEFAULT_SPI pins and 10MHz.
     * @param cfg Pointer to config struct to populate.
     * @return true if configuration is valid.
     */
    bool hly_sd_generate_std_config(hly_sd_config_t *cfg);

    /**
     * @brief Generates a configuration based on a starting RX pin and target baud.
     * @note Assumes standard pin mapping: CS=RX+1, CLK=RX+2, TX=RX+3.
     * @param rx_pin The MISO pin number.
     * @param baud Desired SPI frequency in Hz.
     * @param cfg Pointer to config struct to populate.
     * @return true if configuration is valid.
     */
    bool hly_sd_generate_config(const uint8_t rx_pin, const uint32_t baud, hly_sd_config_t *cfg);

    /**
     * @brief Initializes the SD card and switches to SPI mode.
     * @param cfg Pointer to the initialized configuration.
     * @return HLY_SD_OK on success, negative error code otherwise.
     */
    int hly_sd_init(hly_sd_config_t *cfg);

    /**
     * @brief Reads a single 512-byte block from the card.
     * @param cfg Pointer to the config handle.
     * @param block The block address to read.
     * @param buf Buffer to store the 512 bytes (must be allocated).
     * @param crc Optional pointer to store the received CRC16.
     * @return HLY_SD_OK on success, negative error code otherwise.
     */
    int hly_sd_read_block(const hly_sd_config_t *cfg, uint32_t block, uint8_t *buf, uint16_t *crc);

    /**
     * @brief Writes a single 512-byte block to the card.
     * @param cfg Pointer to the config handle.
     * @param block The block address to write.
     * @param buf Buffer containing 512 bytes of data.
     * @return HLY_SD_OK on success, negative error code otherwise.
     */
    int hly_sd_write_block(const hly_sd_config_t *cfg, uint32_t block, uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif