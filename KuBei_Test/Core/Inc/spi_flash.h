/**
  ******************************************************************************
  * @file    spi_flash.h
  * @brief   通用 SPI NOR Flash 底层驱动（SPI1/SPI2/SPI3 都可用，各挂一个器件）
  *
  *  说明：只做"总线 + Flash 时序"这一层，不含任何具体芯片容量假设。
  *        器件层见 w25q64.c（SPI1 上的 25Q64）与 font_lib.c（SPI2 上的字库 Flash）。
  *        指令集符合标准 SPI NOR Flash（W25Qxx / GT20L16 字库芯片 等通用）。
  ******************************************************************************
  */

#ifndef SPI_FLASH_H
#define SPI_FLASH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "spi.h"

/* ---------------- 标准 SPI Flash 指令 ---------------- */
#define SPIFLASH_CMD_WRITE_ENABLE    0x06u   /* 写使能 */
#define SPIFLASH_CMD_READ_STATUS1    0x05u   /* 读状态寄存器1 */
#define SPIFLASH_CMD_READ_DATA       0x03u   /* 读数据 */
#define SPIFLASH_CMD_PAGE_PROGRAM    0x02u   /* 页编程 */
#define SPIFLASH_CMD_SECTOR_ERASE    0x20u   /* 4KB 扇区擦除 */
#define SPIFLASH_CMD_BLOCK_ERASE     0xD8u   /* 64KB 块擦除 */
#define SPIFLASH_CMD_CHIP_ERASE      0xC7u   /* 整片擦除 */
#define SPIFLASH_CMD_JEDEC_ID        0x9Fu   /* 读 JEDEC ID */
#define SPIFLASH_CMD_RELEASE_PD      0xABu   /* 退出掉电 */

/* 状态寄存器 1 位定义 */
#define SPIFLASH_SR1_BUSY            0x01u   /* 1 = 正在擦写 */
#define SPIFLASH_SR1_WEL             0x02u   /* 1 = 写使能已置位 */

/* 返回值 */
#define SPIFLASH_OK                  0u
#define SPIFLASH_ERROR               1u
#define SPIFLASH_TIMEOUT             2u

#define SPIFLASH_PAGE_SIZE           256u
#define SPIFLASH_SECTOR_SIZE         4096u
#define SPIFLASH_BLOCK64_SIZE        65536u

/**
  * @brief  一个 Flash 器件实例（把 SPI 句柄和片选脚绑在一起）
  */
typedef struct
{
    SPI_HandleTypeDef *hspi;      /* 所挂的 SPI（&hspi1 / &hspi2 ...） */
    GPIO_TypeDef      *cs_port;   /* 片选端口 */
    uint16_t           cs_pin;    /* 片选引脚 */
    uint32_t           size_kb;   /* 容量（KB），用于地址越界检查 */
    uint8_t            attached;  /* 是否已挂载 */
} SpiFlash_t;

/* ---------------- 底层接口 ---------------- */
void     SpiFlash_Attach(SpiFlash_t *dev, SPI_HandleTypeDef *hspi,
                         GPIO_TypeDef *cs_port, uint16_t cs_pin, uint32_t size_kb);

void     SpiFlash_Select(SpiFlash_t *dev);
void     SpiFlash_Deselect(SpiFlash_t *dev);

uint8_t  SpiFlash_ReadStatus1(SpiFlash_t *dev);
uint8_t  SpiFlash_IsBusy(SpiFlash_t *dev);
uint8_t  SpiFlash_WaitReady(SpiFlash_t *dev, uint32_t timeout_ms);

uint32_t SpiFlash_ReadJedecID(SpiFlash_t *dev);      /* 0xEF4017 = W25Q64 */

uint8_t  SpiFlash_Read(SpiFlash_t *dev, uint32_t addr, uint8_t *buf, uint32_t len);
uint8_t  SpiFlash_PageProgram(SpiFlash_t *dev, uint32_t addr,
                              const uint8_t *buf, uint32_t len);   /* len <= 256，不跨页 */
uint8_t  SpiFlash_Write(SpiFlash_t *dev, uint32_t addr,
                        const uint8_t *buf, uint32_t len);         /* 自动分页，不负责擦除 */

uint8_t  SpiFlash_EraseSector(SpiFlash_t *dev, uint32_t addr);     /* 4KB */
uint8_t  SpiFlash_EraseBlock64(SpiFlash_t *dev, uint32_t addr);    /* 64KB */
uint8_t  SpiFlash_EraseChip(SpiFlash_t *dev);

/* 从 addr 起找第一个不是 0xFF 的字节位置；返回偏移，-1 表示指定范围全为 0xFF（含读失败） */
int32_t  SpiFlash_FirstNonFF(SpiFlash_t *dev, uint32_t addr, uint32_t len);

/* 读回并与数据比较，返回不一致的字节数（0 = 完全一致） */
uint32_t SpiFlash_Compare(SpiFlash_t *dev, uint32_t addr, const uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* SPI_FLASH_H */
