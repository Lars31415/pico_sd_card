#include "sd_card.h"

#include "crc7.h"
#include "crc16.h"

#include <pico/stdlib.h>
#include <stdio.h>
#include <string.h>

static const uint8_t ff = 0xFF;
static const uint8_t sd_token = 0xFE;
static const uint8_t sd_data_accept = 0x05;

void print_buffer(const uint8_t *bf, uint16_t sz);

static inline bool check_config(sma_sd_config_t *cfg)
{
    switch (cfg->rx_pin)
    {
    case 0:
    case 4:
    case 16:
    case 8:
    case 12:
        break;
    default:
        printf("SPI rx_pin(%d) is invalid\n", cfg->rx_pin);
        return false;
    }
    if (cfg->cs_pin != cfg->rx_pin + 1)
        printf("Warning, SPI cs_pin(%d) is not %d.\n", cfg->cs_pin, cfg->rx_pin + 1);
    if (cfg->clk_pin != cfg->rx_pin + 2)
    {
        printf("SPI clk_pin(%d) is not %d\n.", cfg->clk_pin, cfg->rx_pin + 2);
        return false;
    }
    if (cfg->tx_pin != cfg->rx_pin + 3)
    {
        printf("SPI tx_pin(%d) is not %d\n.", cfg->tx_pin, cfg->rx_pin + 3);
        return false;
    }
    if ((cfg->baud < 100000) || (cfg->baud > 10000000))
        printf("Warning, SPI baud(%u) is out of range 100kHz - 10MHz\n", cfg->baud);
    if (((cfg->rx_pin >> 3) & 0x01) == 1)
    {
        if (spi1 != cfg->spi)
        {
            printf("Wrong spi_inst(spi0)\n");
            return false;
        }
    }
    else
    {
        if (spi0 != cfg->spi)
        {
            printf("Wrong spi_inst(spi1)\n");
            return false;
        }
    }
    return true;
}

bool sma_sd_generate_std_config(sma_sd_config_t *cfg)
{
    return sma_sd_generate_config(PICO_DEFAULT_SPI_RX_PIN, 10000000, cfg);
}

bool sma_sd_generate_config(const uint8_t rx_pin, const uint32_t baud, sma_sd_config_t *cfg)
{
    memset(cfg, 0, sizeof(sma_sd_config_t));
    bool is_spi1 = ((rx_pin >> 3) & 0x01);
    cfg->spi = is_spi1 ? spi1 : spi0;
    cfg->rx_pin = rx_pin;
    cfg->cs_pin = rx_pin + 1;
    cfg->clk_pin = rx_pin + 2;
    cfg->tx_pin = rx_pin + 3;
    cfg->baud = baud;
    return check_config(cfg);
}

static inline bool check_range(sma_sd_config_t *cfg, uint32_t bn)
{
    if (bn >= cfg->desc.block_count)
        return false;
    return true;
}

static inline void encode_addr(sma_sd_config_t *cfg, uint32_t addr, uint8_t *p)
{
    if (!cfg->desc.sdhc)
        addr *= 512;
    *(p) = (uint8_t)(addr >> 24); // MSB
    *(p + 1) = (uint8_t)(addr >> 16);
    *(p + 2) = (uint8_t)(addr >> 8);
    *(p + 3) = (uint8_t)(addr);
}

static inline bool sd_wait_ready(const sma_sd_config_t *cfg, uint32_t timeout_ms)
{
    uint8_t res;
    absolute_time_t timeout_time = make_timeout_time_ms(timeout_ms);
    do
    {
        spi_write_read_blocking(cfg->spi, &ff, &res, 1);
        if (res == 0xFF)
            return true; // Card is ready
    } while (absolute_time_diff_us(get_absolute_time(), timeout_time) > 0);
    return false; // Card stayed busy (DO low)
}

static inline uint8_t sd_wait_r1(const sma_sd_config_t *cfg)
{
    uint8_t r = 0xFF;
    uint8_t ff = 0xFF;

    for (int i = 0; i < 1000; i++)
    {
        spi_write_read_blocking(cfg->spi, &ff, &r, 1);
        if ((r & 0x80) == 0)
            return r;
    }
    printf("r1 timeout, last: 0x%02X\n", r);
    return r;
}

static inline uint8_t sd_wait_token(const sma_sd_config_t *cfg)
{
    uint8_t r;
    for (int i = 0; i < 100000; i++)
    {
        spi_write_read_blocking(cfg->spi, &ff, &r, 1);
        if (r == sd_token) // bit7 cleared
            return r;
    }
    printf("token error: 0x%02X\n", r);
    return r; // timeout
}

static inline void sd_encode_cmd(uint8_t cmd[])
{
    cmd[0] |= 0x40;
    if ((cmd[0] == 0x40) || (cmd[0] == 0x48))
        cmd[5] = mmc_sdc_crc7(cmd, 5);
    else
        cmd[5] = 0xFF;
}

static int sd_cmd(const sma_sd_config_t *cfg, uint8_t cmd[], uint8_t rep[], uint16_t len)
{
    sd_encode_cmd(cmd);

    // Send commend
    gpio_put(cfg->cs_pin, 0);

    if (!sd_wait_ready(cfg, 500))
    {
        gpio_put(cfg->cs_pin, 1);
        printf("CMD%d card ready timeout.", cmd[0] & 0x3F);
        return SMA_SD_BUSY; // Or a dedicated SMA_SD_BUSY error
    }

    spi_write_blocking(cfg->spi, cmd, 6);
    // flush
    spi_write_blocking(cfg->spi, &ff, 1);

    // Wait for response
    uint8_t r1 = sd_wait_r1(cfg);

    if (rep == NULL)
    {
        gpio_put(cfg->cs_pin, 1);
        spi_write_blocking(cfg->spi, &ff, 1);
        return r1;
    }

    int rl = spi_read_blocking(cfg->spi, ff, rep, len);
    gpio_put(cfg->cs_pin, 1);
    spi_write_blocking(cfg->spi, &ff, 1);

    return r1;
}

int sma_sd_read_block(sma_sd_config_t *cfg, uint32_t block, uint8_t *buf, uint16_t *crc)
{
    memset(buf, 0, sma_sd_block_size);

    if (!check_range(cfg, block))
    {
        printf("CMD17 failed, range error %lu >= %lu\n", block, cfg->desc.block_count);
        return SMA_SD_RANGE;
    }

    uint8_t cmd[] = {0x11, 0, 0, 0, 0, 0};
    encode_addr(cfg, block, cmd + 1);
    sd_encode_cmd(cmd);

    print_buffer(cmd, 6);

    gpio_put(cfg->cs_pin, 0);

    if (!sd_wait_ready(cfg, 500))
    {
        gpio_put(cfg->cs_pin, 1);
        printf("CMD17 card ready timeout.");
        return SMA_SD_BUSY; // Or a dedicated SMA_SD_BUSY error
    }

    // Send command
    spi_write_blocking(cfg->spi, cmd, 6);

    // Clock until R1 appears
    uint8_t r1 = sd_wait_r1(cfg);
    if (r1 != 0x00)
    {
        gpio_put(cfg->cs_pin, 1);
        spi_write_blocking(cfg->spi, &ff, 1);
        printf("CMD17 failed, R1=0x%02X\n", r1);
        return SMA_SD_R1;
    }

    uint8_t token = sd_wait_token(cfg);
    if (token != sd_token)
    {
        gpio_put(cfg->cs_pin, 1);
        spi_write_blocking(cfg->spi, &ff, 1);
        printf("CMD17 failed, token=0x%02X\n", token);
        return SMA_SD_TOKEN;
    }

    // Read data
    uint rl = spi_read_blocking(cfg->spi, ff, buf, sma_sd_block_size);
    uint8_t crc_bytes[2];
    spi_read_blocking(cfg->spi, ff, crc_bytes, 2);
    gpio_put(cfg->cs_pin, 1);
    spi_write_blocking(cfg->spi, &ff, 1);
    if (rl != sma_sd_block_size)
    {
        printf("CMD17 failed, %u bytes received\n", rl);
        return SMA_SD_INCON;
    }

    uint16_t read_crc = ((uint16_t)crc_bytes[0] << 8) | crc_bytes[1];
    uint16_t calc_crc = crc16(buf, sma_sd_block_size, 0);
    if (read_crc != calc_crc)
    {
        printf("CMD17 failed, CRC error 0x%04X != 0x%04X\n", read_crc, calc_crc);
        return SMA_SD_CRC;
    }

    if (crc)
        *crc = read_crc;

    return SMA_SD_OK;
}

int sma_sd_write_block(sma_sd_config_t *cfg, uint32_t block, uint8_t *buf)
{
    if (!check_range(cfg, block))
    {
        printf("CMD24 failed, range error %lu >= %lu\n", block, cfg->desc.block_count);
        return SMA_SD_RANGE;
    }

    uint8_t cmd[] = {0x18, 0, 0, 0, 0, 0};
    encode_addr(cfg, block, cmd + 1);
    sd_encode_cmd(cmd);

    gpio_put(cfg->cs_pin, 0);

    if (!sd_wait_ready(cfg, 500))
    {
        gpio_put(cfg->cs_pin, 1);
        printf("CMD24 card ready timeout.");
        return SMA_SD_BUSY; // Or a dedicated SMA_SD_BUSY error
    }

    // Send command
    spi_write_blocking(cfg->spi, cmd, 6);

    // Clock until R1 appears
    uint8_t r1 = sd_wait_r1(cfg);
    if (r1 != 0x00)
    {
        gpio_put(cfg->cs_pin, 1);
        spi_write_blocking(cfg->spi, &ff, 1);
        printf("CMD24 failed, R1=0x%02X\n", r1);
        return SMA_SD_R1;
    }

    spi_write_blocking(cfg->spi, &sd_token, 1);

    uint wl = spi_write_blocking(cfg->spi, buf, sma_sd_block_size);

    uint16_t crc = change_order(crc16(buf, sma_sd_block_size, 0));
    spi_write_blocking(cfg->spi, (uint8_t *)&crc, 2);

    if (wl != sma_sd_block_size)
    {
        gpio_put(cfg->cs_pin, 1);
        spi_write_blocking(cfg->spi, &ff, 1);
        printf("CMD24 failed, %u bytes writen\n", wl);
        return SMA_SD_INCON;
    }

    uint8_t resp;
    spi_write_read_blocking(cfg->spi, &ff, &resp, 1);

    if ((resp & 0x1F) != sd_data_accept)
    {
        gpio_put(cfg->cs_pin, 1);
        spi_write_blocking(cfg->spi, &ff, 1);
        printf("CMD24 failed, status %u\n", (resp & 0x1F));
        return SMA_SD_WRITE;
    }

    uint8_t busy;
    uint32_t timeout = 500000; // Large timeout for flash programming
    do
    {
        spi_write_read_blocking(cfg->spi, &ff, &busy, 1); //
        if (--timeout == 0)
        {
            gpio_put(cfg->cs_pin, 1);
            printf("CMD24 timeout waiting for busy release\n");
            return SMA_SD_WRITE;
        }
    } while (busy != 0xFF);

    gpio_put(cfg->cs_pin, 1);
    spi_write_blocking(cfg->spi, &ff, 1);

    return SMA_SD_OK;
}

static inline int sd_read_csd(const sma_sd_config_t *cfg, uint8_t *csd)
{
    uint8_t cmd[] = {0x09, 0, 0, 0, 0, 0};
    sd_encode_cmd(cmd);

    gpio_put(cfg->cs_pin, 0);

    if (!sd_wait_ready(cfg, 500))
    {
        gpio_put(cfg->cs_pin, 1);
        printf("CMD9 card ready timeout.");
        return SMA_SD_BUSY; // Or a dedicated SMA_SD_BUSY error
    }

    spi_write_blocking(cfg->spi, cmd, 6);

    uint r1 = sd_wait_r1(cfg);
    if (r1 != 0x00)
    {
        gpio_put(cfg->cs_pin, 1);
        printf("CMD9 failed, R1=0x%02X\n", r1);
        return -1;
    }

    uint token = sd_wait_token(cfg);
    if (token != sd_token)
    {
        gpio_put(cfg->cs_pin, 1);
        printf("CMD9 failed, token=0x%02X\n", token);
        return -2;
    }

    uint rl = spi_read_blocking(cfg->spi, ff, csd, 16);
    uint16_t crc;
    spi_read_blocking(cfg->spi, ff, (uint8_t *)&crc, 2);

    gpio_put(cfg->cs_pin, 1);
    spi_write_blocking(cfg->spi, &ff, 1);
    if (rl != 16)
    {
        printf("CMD9 failed, %u bytes received\n", rl);
        return -3;
    }
    return 0;
}

int sma_sd_init(sma_sd_config_t *cfg)
{
    if (!check_config(cfg))
        return -1;
    // printf("%s %d\n", __FUNCTION__, __LINSMA_SD__);
    // This example will use SPI0 at 0.5MHz.
    spi_init(cfg->spi, 400 * 1000);

    gpio_set_function(cfg->rx_pin, GPIO_FUNC_SPI);
    gpio_set_function(cfg->clk_pin, GPIO_FUNC_SPI);
    gpio_set_function(cfg->tx_pin, GPIO_FUNC_SPI);
    gpio_put(cfg->cs_pin, 1);

    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_init(cfg->cs_pin);
    gpio_set_dir(cfg->cs_pin, GPIO_OUT);
    gpio_put(cfg->cs_pin, 1);

    gpio_pull_up(cfg->rx_pin);

    gpio_put(cfg->cs_pin, 1);
    for (int i = 0; i < 10; i++)
        spi_write_blocking(cfg->spi, &ff, 1);

    // CMD0
    uint8_t cmd0[] = {0x00, 0, 0, 0, 0, 0};
    sd_cmd(cfg, cmd0, NULL, 0);

    // CMD8
    uint8_t cmd8[] = {0x08, 0, 0, 0x01, 0xAA, 0};
    uint8_t r7[4];
    sd_cmd(cfg, cmd8, r7, 4);

    // CMD55
    uint8_t cmd55[] = {0x37, 0, 0, 0, 0, 0};
    uint8_t acmd41[] = {0x29, 0x40, 0, 0, 0, 0};
    uint8_t r;
    absolute_time_t timeout_time = make_timeout_time_ms(1000);
    do
    {
        sd_cmd(cfg, cmd55, NULL, 0);
        r = sd_cmd(cfg, acmd41, NULL, 0);
        if (absolute_time_diff_us(get_absolute_time(), timeout_time) <= 0)
        {
            return SMA_SD_TIMEOUT;
        }
    } while (r != 0x00);

    // CMD58
    uint8_t cmd58[] = {0x3A, 0, 0, 0, 0, 0};
    uint8_t ocr[4];
    sd_cmd(cfg, cmd58, ocr, 4);
    cfg->desc.sdhc = ((ocr[0] & 0x40) != 0);

    uint8_t csd[16];
    sd_read_csd(cfg, csd);
    cfg->desc.csd_structure = (csd[0] >> 6) & 0x03;
    if (cfg->desc.csd_structure == 1)
    {
        cfg->desc.c_size =
            ((uint32_t)(csd[7] & 0x3F) << 16) |
            ((uint32_t)csd[8] << 8) |
            csd[9];
        cfg->desc.block_count = (cfg->desc.c_size + 1) * 1024;
        cfg->desc.card_size = (cfg->desc.block_count << 9);
    }

    // CMD16
    uint8_t cmd16[] = {0x10, 0, 0, 0x02, 0, 0};
    sd_cmd(cfg, cmd16, NULL, 0);

    spi_set_baudrate(cfg->spi, cfg->baud);

    return SMA_SD_OK;
}
