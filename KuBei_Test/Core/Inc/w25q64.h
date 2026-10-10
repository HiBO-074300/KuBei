/**
  ******************************************************************************
  * @file    w25q64.h
  * @brief   SPI1 上的 W25Q64（8MB，64Mbit）串行 Flash 驱动
  *
  *  硬件：SPI1 = PA5/PA6/PA7，片选 FLASH1_CS = PA4（见 main.h）
  *  说明：读写擦全部走 spi_flash.c 的通用底层，本文件只做器件层的封装与自检。
  ******************************************************************************
  */

#ifndef W25Q64_H
#define W25Q64_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "spi_flash.h"

/* ---------------- 器件参数 ---------------- */
#define W25Q64_SIZE_KB        8192u      /* 8MB */
#define W25Q64_PAGE_SIZE      256u
#define W25Q64_SECTOR_SIZE    4096u      /* 最小擦除单位 */
#define W25Q64_BLOCK64_SIZE   65536u
#define W25Q64_JEDEC_ID       0xEF4017u  /* Winbond W25Q64 */

/* 自检使用的地址：默认用最后一个扇区，不碰前面数据，可用宏改到别的扇区 */
#ifndef W25Q64_TEST_ADDR
#define W25Q64_TEST_ADDR      (W25Q64_SIZE_KB * 1024u - W25Q64_SECTOR_SIZE)
#endif

/* ---------------- 接口 ---------------- */
/* 挂载到 SPI1 + FLASH1_CS，返回供其它模块直接使用的底层句柄 */
SpiFlash_t *W25Q64_Handle(void);

/* 初始化（片选拉高 + 读 ID），返回 0 = 找到 25Q64，其它 = 未识别/通信失败 */
uint8_t  W25Q64_Init(void);

uint8_t  W25Q64_IsPresent(void);                  /* 1 = JEDEC ID 匹配 W25Q64 */
uint32_t W25Q64_GetID(void);                      /* 上次读到的 JEDEC ID */
uint32_t W25Q64_ReadID(void);                     /* 重新读一次 */

uint8_t  W25Q64_Read(uint32_t addr, uint8_t *buf, uint32_t len);
uint8_t  W25Q64_Write(uint32_t addr, const uint8_t *buf, uint32_t len); /* 需先擦除 */
uint8_t  W25Q64_EraseSector(uint32_t addr);       /* 4KB */
uint8_t  W25Q64_EraseChip(void);

int32_t  W25Q64_FirstNonFF(uint32_t addr, uint32_t len);
uint32_t W25Q64_Compare(uint32_t addr, const uint8_t *buf, uint32_t len);

/* 上电自检：擦除 W25Q64_TEST_ADDR 扇区 → 写测试图案 → 读回比较 → 再擦干净
 * 返回 0 = 通过（读写擦全正常）；其它 = 失败，可用 W25Q64_LastError() 看原因 */
uint8_t  W25Q64_SelfTest(void);
uint8_t  W25Q64_LastError(void);

/* 打印信息用：把 ID 拆成厂商/容量字符串，例如 "EF 40 17 (Winbond 8MB)" */
const char *W25Q64_DescribeID(uint32_t id);

#ifdef __cplusplus
}
#endif

#endif /* W25Q64_H */
