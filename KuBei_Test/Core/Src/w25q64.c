/**
  ******************************************************************************
  * @file    w25q64.c
  * @brief   SPI1 上的 W25Q64 器件驱动（8MB）
  *
  *  片选：FLASH1_CS = PA4（CubeMX 已配成推挽输出，上电默认低，这里会先拉高）
  ******************************************************************************
  */

#include "w25q64.h"
#include "spi.h"
#include <string.h>

static SpiFlash_t s_w25q64;
static uint32_t   s_id       = 0;
static uint8_t    s_present  = 0;
static uint8_t    s_lastErr  = SPIFLASH_OK;

SpiFlash_t *W25Q64_Handle(void)
{
    return &s_w25q64;
}

uint32_t W25Q64_ReadID(void)
{
    s_id = SpiFlash_ReadJedecID(&s_w25q64);
    return s_id;
}

uint32_t W25Q64_GetID(void)
{
    return s_id;
}

uint8_t W25Q64_IsPresent(void)
{
    return s_present;
}

uint8_t W25Q64_LastError(void)
{
    return s_lastErr;
}

uint8_t W25Q64_Init(void)
{
    SpiFlash_Attach(&s_w25q64, &hspi1, FLASH1_CS_GPIO_Port, FLASH1_CS_Pin, W25Q64_SIZE_KB);

    s_id      = SpiFlash_ReadJedecID(&s_w25q64);
    s_present = (s_id == W25Q64_JEDEC_ID) ? 1u : 0u;

    return s_present ? 0u : 1u;
}

uint8_t W25Q64_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    s_lastErr = SpiFlash_Read(&s_w25q64, addr, buf, len);
    return s_lastErr;
}

uint8_t W25Q64_Write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    s_lastErr = SpiFlash_Write(&s_w25q64, addr, buf, len);
    return s_lastErr;
}

uint8_t W25Q64_EraseSector(uint32_t addr)
{
    s_lastErr = SpiFlash_EraseSector(&s_w25q64, addr);
    return s_lastErr;
}

uint8_t W25Q64_EraseChip(void)
{
    s_lastErr = SpiFlash_EraseChip(&s_w25q64);
    return s_lastErr;
}

int32_t W25Q64_FirstNonFF(uint32_t addr, uint32_t len)
{
    return SpiFlash_FirstNonFF(&s_w25q64, addr, len);
}

uint32_t W25Q64_Compare(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    return SpiFlash_Compare(&s_w25q64, addr, buf, len);
}

uint8_t W25Q64_SelfTest(void)
{
    static const uint8_t pattern[16] = {
        0x55, 0xAA, 0xA5, 0x5A, 0x00, 0xFF, 0x12, 0x34,
        0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x0F, 0xC3
    };
    uint8_t  rd[16];
    uint32_t i;

    s_lastErr = SPIFLASH_OK;

    if (!s_present) { s_lastErr = SPIFLASH_ERROR; return 1u; }   /* 器件不存在 */

    /* 1. 擦除测试扇区 */
    s_lastErr = SpiFlash_EraseSector(&s_w25q64, W25Q64_TEST_ADDR);
    if (s_lastErr != SPIFLASH_OK) return 2u;

    /* 2. 擦完必须是全 0xFF */
    if (SpiFlash_FirstNonFF(&s_w25q64, W25Q64_TEST_ADDR, W25Q64_SECTOR_SIZE) >= 0)
    {
        s_lastErr = SPIFLASH_ERROR;
        return 3u;
    }

    /* 3. 写入测试图案（16 字节，页内不跨界） */
    s_lastErr = SpiFlash_Write(&s_w25q64, W25Q64_TEST_ADDR, pattern, 16u);
    if (s_lastErr != SPIFLASH_OK) return 4u;

    /* 4. 读回比较 */
    memset(rd, 0, sizeof(rd));
    s_lastErr = SpiFlash_Read(&s_w25q64, W25Q64_TEST_ADDR, rd, 16u);
    if (s_lastErr != SPIFLASH_OK) return 5u;

    for (i = 0; i < 16u; i++)
    {
        if (rd[i] != pattern[i]) { s_lastErr = SPIFLASH_ERROR; return 6u; }
    }

    /* 5. 测试完擦干净，保持 Flash 干净 */
    s_lastErr = SpiFlash_EraseSector(&s_w25q64, W25Q64_TEST_ADDR);
    if (s_lastErr != SPIFLASH_OK) return 7u;

    return 0u;
}

const char *W25Q64_DescribeID(uint32_t id)
{
    switch (id)
    {
        case 0xEF4017u: return "Winbond W25Q64 8MB";
        case 0xEF4018u: return "Winbond W25Q128 16MB";
        case 0xEF4016u: return "Winbond W25Q32 4MB";
        case 0xEF4015u: return "Winbond W25Q16 2MB";
        case 0xC84017u: return "GigaDevice GD25Q64 8MB";
        case 0xC22017u: return "Macronix MX25L64 8MB";
        case 0x1C3117u: return "ESMT/其它 25Q64 8MB";
        default:        return "unknown";
    }
}
