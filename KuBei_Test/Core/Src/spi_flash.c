/**
  ******************************************************************************
  * @file    spi_flash.c
  * @brief   通用 SPI NOR Flash 底层驱动实现
  *
  *  时序：所有指令（含地址）用一次 SPI 发送，然后按需接收数据；
  *        擦写前后按状态寄存器 BUSY 位等待，片选由 CS 引脚手动控制。
  ******************************************************************************
  */

#include "spi_flash.h"

#define SPIFLASH_CS_LOW(dev)    HAL_GPIO_WritePin((dev)->cs_port, (dev)->cs_pin, GPIO_PIN_RESET)
#define SPIFLASH_CS_HIGH(dev)   HAL_GPIO_WritePin((dev)->cs_port, (dev)->cs_pin, GPIO_PIN_SET)
#define SPIFLASH_POLL_MS        1u

void SpiFlash_Attach(SpiFlash_t *dev, SPI_HandleTypeDef *hspi,
                     GPIO_TypeDef *cs_port, uint16_t cs_pin, uint32_t size_kb)
{
    if (dev == NULL) return;

    dev->hspi     = hspi;
    dev->cs_port  = cs_port;
    dev->cs_pin   = cs_pin;
    dev->size_kb  = size_kb;
    dev->attached = 1u;

    SPIFLASH_CS_HIGH(dev);
}

void SpiFlash_Select(SpiFlash_t *dev)
{
    SPIFLASH_CS_LOW(dev);
}

void SpiFlash_Deselect(SpiFlash_t *dev)
{
    SPIFLASH_CS_HIGH(dev);
}

/* 发送 指令 + 3 字节地址（读状态这类无地址指令请直接用 SpiFlash_Select + HAL_SPI_Transmit） */
static void SpiFlash_SendCmd24(SpiFlash_t *dev, uint8_t cmd, uint32_t addr)
{
    uint8_t buf[4];

    buf[0] = cmd;
    buf[1] = (uint8_t)((addr >> 16) & 0xFF);
    buf[2] = (uint8_t)((addr >> 8) & 0xFF);
    buf[3] = (uint8_t)(addr & 0xFF);

    SpiFlash_Select(dev);
    HAL_SPI_Transmit(dev->hspi, buf, 4, HAL_MAX_DELAY);
}

uint8_t SpiFlash_ReadStatus1(SpiFlash_t *dev)
{
    uint8_t cmd = SPIFLASH_CMD_READ_STATUS1;
    uint8_t sr = 0xFF;

    if (dev == NULL || !dev->attached) return 0xFFu;

    SpiFlash_Select(dev);
    HAL_SPI_Transmit(dev->hspi, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(dev->hspi, &sr, 1, HAL_MAX_DELAY);
    SpiFlash_Deselect(dev);

    return sr;
}

uint8_t SpiFlash_IsBusy(SpiFlash_t *dev)
{
    return (SpiFlash_ReadStatus1(dev) & SPIFLASH_SR1_BUSY) ? 1u : 0u;
}

uint8_t SpiFlash_WaitReady(SpiFlash_t *dev, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while (SpiFlash_IsBusy(dev))
    {
        if ((HAL_GetTick() - start) > timeout_ms) return SPIFLASH_TIMEOUT;
        HAL_Delay(SPIFLASH_POLL_MS);
    }
    return SPIFLASH_OK;
}

static void SpiFlash_WriteEnable(SpiFlash_t *dev)
{
    uint8_t cmd = SPIFLASH_CMD_WRITE_ENABLE;

    SpiFlash_Select(dev);
    HAL_SPI_Transmit(dev->hspi, &cmd, 1, HAL_MAX_DELAY);
    SpiFlash_Deselect(dev);
}

uint32_t SpiFlash_ReadJedecID(SpiFlash_t *dev)
{
    uint8_t cmd = SPIFLASH_CMD_JEDEC_ID;
    uint8_t id[3] = {0xFF, 0xFF, 0xFF};

    if (dev == NULL || !dev->attached) return 0xFFFFFFFFu;

    SpiFlash_Select(dev);
    HAL_SPI_Transmit(dev->hspi, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(dev->hspi, id, 3, HAL_MAX_DELAY);
    SpiFlash_Deselect(dev);

    return ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | (uint32_t)id[2];
}

uint8_t SpiFlash_Read(SpiFlash_t *dev, uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint32_t chunk;

    if (dev == NULL || !dev->attached || buf == NULL || len == 0u) return SPIFLASH_ERROR;
    if ((addr + len) > (dev->size_kb * 1024u)) return SPIFLASH_ERROR;

    SpiFlash_SendCmd24(dev, SPIFLASH_CMD_READ_DATA, addr);

    /* HAL 的收发长度参数是 16 位，超长时分段收 */
    while (len > 0u)
    {
        chunk = (len > 0xFFFFu) ? 0xFFFFu : len;
        HAL_SPI_Receive(dev->hspi, buf, (uint16_t)chunk, HAL_MAX_DELAY);
        buf += chunk;
        len -= chunk;
    }

    SpiFlash_Deselect(dev);

    return SPIFLASH_OK;
}

uint8_t SpiFlash_PageProgram(SpiFlash_t *dev, uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint8_t  head[4];
    uint32_t pageLeft;

    if (dev == NULL || !dev->attached || buf == NULL || len == 0u) return SPIFLASH_ERROR;
    if (len > SPIFLASH_PAGE_SIZE) return SPIFLASH_ERROR;
    if ((addr + len) > (dev->size_kb * 1024u)) return SPIFLASH_ERROR;

    pageLeft = SPIFLASH_PAGE_SIZE - (addr % SPIFLASH_PAGE_SIZE);
    if (len > pageLeft) return SPIFLASH_ERROR;      /* 不允许跨页 */

    SpiFlash_WriteEnable(dev);

    head[0] = SPIFLASH_CMD_PAGE_PROGRAM;
    head[1] = (uint8_t)((addr >> 16) & 0xFF);
    head[2] = (uint8_t)((addr >> 8) & 0xFF);
    head[3] = (uint8_t)(addr & 0xFF);

    SpiFlash_Select(dev);
    HAL_SPI_Transmit(dev->hspi, head, 4, HAL_MAX_DELAY);
    HAL_SPI_Transmit(dev->hspi, (uint8_t *)buf, (uint16_t)len, HAL_MAX_DELAY);
    SpiFlash_Deselect(dev);

    return SpiFlash_WaitReady(dev, 100u);
}

uint8_t SpiFlash_Write(SpiFlash_t *dev, uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t pageLeft;
    uint32_t chunk;
    uint8_t  ret;

    if (dev == NULL || !dev->attached || buf == NULL || len == 0u) return SPIFLASH_ERROR;

    while (len > 0u)
    {
        pageLeft = SPIFLASH_PAGE_SIZE - (addr % SPIFLASH_PAGE_SIZE);
        chunk    = (len < pageLeft) ? len : pageLeft;

        ret = SpiFlash_PageProgram(dev, addr, buf, chunk);
        if (ret != SPIFLASH_OK) return ret;

        addr += chunk;
        buf  += chunk;
        len  -= chunk;
    }
    return SPIFLASH_OK;
}

uint8_t SpiFlash_EraseSector(SpiFlash_t *dev, uint32_t addr)
{
    if (dev == NULL || !dev->attached) return SPIFLASH_ERROR;

    SpiFlash_WriteEnable(dev);
    SpiFlash_SendCmd24(dev, SPIFLASH_CMD_SECTOR_ERASE, addr);
    SpiFlash_Deselect(dev);

    return SpiFlash_WaitReady(dev, 2000u);
}

uint8_t SpiFlash_EraseBlock64(SpiFlash_t *dev, uint32_t addr)
{
    if (dev == NULL || !dev->attached) return SPIFLASH_ERROR;

    SpiFlash_WriteEnable(dev);
    SpiFlash_SendCmd24(dev, SPIFLASH_CMD_BLOCK_ERASE, addr);
    SpiFlash_Deselect(dev);

    return SpiFlash_WaitReady(dev, 5000u);
}

uint8_t SpiFlash_EraseChip(SpiFlash_t *dev)
{
    uint8_t cmd = SPIFLASH_CMD_CHIP_ERASE;

    if (dev == NULL || !dev->attached) return SPIFLASH_ERROR;

    SpiFlash_WriteEnable(dev);
    SpiFlash_Select(dev);
    HAL_SPI_Transmit(dev->hspi, &cmd, 1, HAL_MAX_DELAY);
    SpiFlash_Deselect(dev);

    return SpiFlash_WaitReady(dev, 120000u);      /* 8MB 整片擦除可能要几十秒 */
}

int32_t SpiFlash_FirstNonFF(SpiFlash_t *dev, uint32_t addr, uint32_t len)
{
    uint8_t  buf[64];
    uint32_t done = 0;
    uint32_t chunk, i;

    while (done < len)
    {
        chunk = len - done;
        if (chunk > sizeof(buf)) chunk = sizeof(buf);

        if (SpiFlash_Read(dev, addr + done, buf, chunk) != SPIFLASH_OK) return -1;

        for (i = 0; i < chunk; i++)
        {
            if (buf[i] != 0xFFu) return (int32_t)(done + i);
        }
        done += chunk;
    }
    return -1;
}

uint32_t SpiFlash_Compare(SpiFlash_t *dev, uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint8_t  tmp[64];
    uint32_t done = 0;
    uint32_t chunk, i, bad = 0;

    if (buf == NULL) return len;

    while (done < len)
    {
        chunk = len - done;
        if (chunk > sizeof(tmp)) chunk = sizeof(tmp);

        if (SpiFlash_Read(dev, addr + done, tmp, chunk) != SPIFLASH_OK) return len - done;

        for (i = 0; i < chunk; i++)
        {
            if (tmp[i] != buf[done + i]) bad++;
        }
        done += chunk;
    }
    return bad;
}
