#include "sd_card.h"

#include "crc7.h"
#include "crc16.h"

#include <pico/stdlib.h>
#include <stdio.h>
#include <string.h>

void print_buffer(const uint8_t *bf, uint16_t sz);

static const uint8_t ff = 0xFF;

static inline bool check_config(sd_config_t *cfg)
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
    cfg->spi = spi0;
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

int generate_std_config(sd_config_t *cfg)
{
    return generate_config(PICO_DEFAULT_SPI_RX_PIN, 5000000, cfg);
}

int generate_config(const uint8_t rx_pin, const uint32_t baud, sd_config_t *cfg)
{
    memset(cfg, 0, sizeof(sd_config_t));
    cfg->spi = spi0;
    if (((rx_pin >> 3) & 0x01) == 1)
        cfg->spi = spi1;
    cfg->rx_pin = rx_pin;
    cfg->cs_pin = rx_pin + 1;
    cfg->clk_pin = rx_pin + 2;
    cfg->tx_pin = rx_pin + 3;
    cfg->baud = 5000000;
    return 0;
}

static inline bool check_range(sd_config_t *cfg, uint32_t bn)
{
    if (bn >= cfg->desc.block_count)
        return false;
    return true;
}

static inline void encode_addr(sd_config_t *cfg, uint32_t addr, uint8_t *p)
{
    if (!cfg->desc.sdhc)
        addr *= 512;
    *(p) = (uint8_t)(addr >> 24); // MSB
    *(p + 1) = (uint8_t)(addr >> 16);
    *(p + 2) = (uint8_t)(addr >> 8);
    *(p + 3) = (uint8_t)(addr);
}

static inline uint8_t sd_wait_r1(const sd_config_t *cfg)
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

static inline uint8_t sd_wait_token(const sd_config_t *cfg)
{
    uint8_t r;
    for (int i = 0; i < 100000; i++)
    {
        spi_write_read_blocking(cfg->spi, &ff, &r, 1);
        if (r == 0xFE) // bit7 cleared
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

static int sd_cmd(const sd_config_t *cfg, uint8_t cmd[], uint8_t rep[], uint16_t len)
{
    sd_encode_cmd(cmd);

    // Send commend
    gpio_put(cfg->cs_pin, 0);
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

int sd_read_block(sd_config_t *cfg, uint32_t block, uint8_t *buf, uint16_t *crc)
{
    memset(buf, 0, block_size);

    if (!check_range(cfg, block))
    {
        printf("CMD17 failed, range error %lu >= %lu\n", block, cfg->desc.block_count);
        return E_RANGE;
    }

    uint8_t cmd[] = {0x11, 0, 0, 0, 0, 0};
    encode_addr(cfg, block, cmd + 1);
    sd_encode_cmd(cmd);

    print_buffer(cmd, 6);

    gpio_put(cfg->cs_pin, 0);

    // Send command
    spi_write_blocking(cfg->spi, cmd, 6);

    // Clock until R1 appears
    uint8_t r1 = sd_wait_r1(cfg);
    if (r1 != 0x00)
    {
        gpio_put(cfg->cs_pin, 1);
        spi_write_blocking(cfg->spi, &ff, 1);
        printf("CMD17 failed, R1=0x%02X\n", r1);
        return E_R1;
    }

    uint8_t token = sd_wait_token(cfg);
    if (token != 0xFE)
    {
        gpio_put(cfg->cs_pin, 1);
        spi_write_blocking(cfg->spi, &ff, 1);
        printf("CMD17 failed, token=0x%02X\n", token);
        return E_TOKEN;
    }

    // Read data
    uint rl = spi_read_blocking(cfg->spi, ff, buf, block_size);
    spi_read_blocking(cfg->spi, ff, (uint8_t *)crc, 2);
    gpio_put(cfg->cs_pin, 1);
    spi_write_blocking(cfg->spi, &ff, 1);
    if (rl != block_size)
    {
        printf("CMD17 failed, %u bytes received\n", rl);
        return E_INCON;
    }
    uint16_t crc1 = change_order(crc16(buf, block_size, 0));
    if (*crc != crc1)
    {
        printf("CMD17 failed, CRC error 0x%02X != 0x%02X \n", crc, crc1);
        return E_CRC;
    }
    return E_OK;
}

int sd_write_block(sd_config_t *cfg, uint16_t block, uint8_t *buf)
{
    const uint8_t token = 0xFE;

    if (!check_range(cfg, block))
    {
        printf("CMD17 failed, range error %lu >= %lu\n", block, cfg->desc.block_count);
        return E_RANGE;
    }

    uint8_t cmd[] = {0x18, 0, 0, 0, 0, 0};
    encode_addr(cfg, block, cmd + 1);
    sd_encode_cmd(cmd);

    gpio_put(cfg->cs_pin, 0);

    // Send command
    spi_write_blocking(cfg->spi, cmd, 6);

    // Clock until R1 appears
    uint8_t r1 = sd_wait_r1(cfg);
    if (r1 != 0x00)
    {
        gpio_put(cfg->cs_pin, 1);
        spi_write_blocking(cfg->spi, &ff, 1);
        printf("CMD24 failed, R1=0x%02X\n", r1);
        return E_R1;
    }

    spi_write_blocking(cfg->spi, &token, 1);

    uint wl = spi_write_blocking(cfg->spi, buf, block_size);

    uint16_t crc = change_order(crc16(buf, block_size, 0));
    spi_write_blocking(cfg->spi, (uint8_t *)&crc, 2);

    int res = 0;
    if (r1 != 0x00)
    {
        gpio_put(cfg->cs_pin, 1);
        spi_write_blocking(cfg->spi, &ff, 1);
        printf("CMD24 failed, %u bytes writen\n", wl);
        res = E_INCON;
    }

    uint8_t resp;
    spi_write_read_blocking(cfg->spi, &ff, &resp, 1);

    if ((resp & 0x1F) != 0x05)
    {
        gpio_put(cfg->cs_pin, 1);
        spi_write_blocking(cfg->spi, &ff, 1);
        printf("CMD24 failed, status %u\n", (resp & 0x1F));
        res = E_WRITE;
    }

    if (res < 0)
    {
        gpio_put(cfg->cs_pin, 1);
        spi_write_blocking(cfg->spi, &ff, 1);
        return res;
    }

    uint8_t busy;
    do
    {
        spi_write_read_blocking(cfg->spi, &ff, &busy, 1);
    } while (busy != 0xFF);

    gpio_put(cfg->cs_pin, 1);
    spi_write_blocking(cfg->spi, &ff, 1);

    return E_OK;
}

int sd_read_csd(const sd_config_t *cfg, uint8_t *csd)
{
    uint8_t cmd[] = {0x09, 0, 0, 0, 0, 0};
    sd_encode_cmd(cmd);

    gpio_put(cfg->cs_pin, 0);
    spi_write_blocking(cfg->spi, cmd, 6);

    uint r1 = sd_wait_r1(cfg);
    if (r1 != 0x00)
    {
        gpio_put(cfg->cs_pin, 1);
        printf("CMD9 failed, R1=0x%02X\n", r1);
        return -1;
    }

    uint token = sd_wait_token(cfg);
    if (token != 0xFE)
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

int sd_init(sd_config_t *cfg)
{
    if (!check_config(cfg))
        return -1;
    // printf("%s %d\n", __FUNCTION__, __LINE__);
    // This example will use SPI0 at 0.5MHz.
    spi_init(spi_default, 500 * 1000);

    gpio_set_function(cfg->rx_pin, GPIO_FUNC_SPI);
    gpio_set_function(cfg->clk_pin, GPIO_FUNC_SPI);
    gpio_set_function(cfg->tx_pin, GPIO_FUNC_SPI);
    gpio_put(cfg->cs_pin, 1);

    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_init(cfg->cs_pin);
    gpio_set_dir(cfg->cs_pin, GPIO_OUT);
    gpio_put(cfg->cs_pin, 1);

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
    do
    {
        sd_cmd(cfg, cmd55, NULL, 0);
        r = sd_cmd(cfg, acmd41, NULL, 0);
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

    spi_set_baudrate(cfg->spi, cfg->baud);

    return 0;
}
