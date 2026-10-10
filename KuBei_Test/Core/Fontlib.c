/**
  ******************************************************************************
  * @文件名		fontlib.c
  * @简介	 	定义函数，实现字库读取文件。
  ******************************************************************************
  * @作者		HiBo
  * @时间		2024-09-25
  * @注意		
  *
  *
  ******************************************************************************
*/
#include "fontlib.h" // 字体库的头文件
#include "spi.h"     // SPI通信的头文件
#include "word_stock.h"


// 从SPI读取一个字节
uint8_t SPI_ReadByte(void)
{
    uint8_t Data;
    HAL_SPI_Receive(&hspi1, &Data, 1, HAL_MAX_DELAY); // 从SPI接口接收一个字节
    return Data; // 返回接收到的数据
}

// 向SPI发送一个字节
void SPI_WriteByte(uint8_t txData)
{
    HAL_SPI_Transmit(&hspi1, &txData, 1, HAL_MAX_DELAY); // 通过SPI接口发送一个字节
}

// 读取一个字体数据
FontSize Read_One_Font(uint16_t addr, uint8_t *data, FontSize datasize)
{
    uint8_t y, x; // 循环变量
    uint32_t address, baseadd; // 地址变量
    uint8_t MSB = addr >> 8, LSB = addr & 0xff; // 将地址分为高8位和低8位

    switch(datasize) // 根据字体大小选择处理方式
    {
        case FONT_SIZE_12x12: // 处理12x12字体
            baseadd = 0x000000; // 基地址
            // 计算地址
            if ((MSB >= 0xA1) && (MSB <= 0xA9) && (LSB >= 0xA1))
            {
                address = (((MSB - 0xA1) * 94 + (LSB - 0xA1)) * 24) + baseadd;
            }
            else if ((MSB >= 0xB0) && (MSB <= 0xF7) && (LSB >= 0xA1))
            {
                address = (((MSB - 0xB0) * 94 + (LSB - 0xA1) + 846) * 24) + baseadd;
            }
            FONT_CS_0; // 选择字体芯片
            SPI_WriteByte(0x03); // 发送命令
            // 发送地址的高、中、低字节
            SPI_WriteByte((address >> 16) & 0xff);
            SPI_WriteByte((address >> 8) & 0xff);
            SPI_WriteByte(address & 0xff);
            // 读取字体数据
            for (y = 0; y < 12; y++)
            {
                for (x = 0; x < 2; x++)
                {
                    data[y * 2 + x] = SPI_ReadByte();
                }
            }
            FONT_CS_1; // 取消选择字体芯片
            return FONT_SIZE_12x12; // 返回字体大小
            break;

        case FONT_SIZE_15x16: // 处理15x16字体
            baseadd = 0x02c9d0; // 基地址
            // 计算地址
            if ((MSB >= 0xA1) && (MSB <= 0xA9) && (LSB >= 0xA1))
            {
                address = (((MSB - 0xA1) * 94 + (LSB - 0xA1)) * 32) + baseadd;
            }
            else if ((MSB >= 0xB0) && (MSB <= 0xF7) && (LSB >= 0xA1))
            {
                address = (((MSB - 0xB0) * 94 + (LSB - 0xA1) + 846) * 32) + baseadd;
            }
            FONT_CS_0; // 选择字体芯片
            SPI_WriteByte(0x03); // 发送命令
            // 发送地址的高、中、低字节
            SPI_WriteByte((address >> 16) & 0xff);
            SPI_WriteByte((address >> 8) & 0xff);
            SPI_WriteByte(address & 0xff);
            // 读取字体数据
            for (y = 0; y < 16; y++)
            {
                for (x = 0; x < 2; x++)
                {
                    data[y * 2 + x] = SPI_ReadByte();
                }
            }
            FONT_CS_1; // 取消选择字体芯片
            return FONT_SIZE_15x16; // 返回字体大小
            break;

        case FONT_SIZE_48x48: // 处理24x24字体
        case FONT_SIZE_24x24: // 处理24x24字体
			  if((MSB == 0xA1)&&((LSB == 0xFD)||(LSB == 0xC1)))
			  {
				  if(LSB == 0xFD)
				  {
					  // 箭头
					  for(y = 0; y < 72; y++)
						data[y] = img2_24[y];
				  }
				  else
				  {
					  // ×
					  for(y = 0; y < 72; y++)
						data[y] = img3_24[y];
					  
				  }
				  break;
			  }
            baseadd = 0x068190; // 基地址
            // 计算地址
            if ((MSB >= 0xA1) && (MSB <= 0xA9) && (LSB >= 0xA1))
            {
                address = (((MSB - 0xA1) * 94 + (LSB - 0xA1)) * 72) + baseadd;
            }
            else if ((MSB >= 0xB0) && (MSB <= 0xF7) && (LSB >= 0xA1))
            {
                address = (((MSB - 0xB0) * 94 + (LSB - 0xA1) + 846) * 72) + baseadd;
            }
            FONT_CS_0; // 选择字体芯片
            SPI_WriteByte(0x03); // 发送命令
            // 发送地址的高、中、低字节
            SPI_WriteByte((address >> 16) & 0xff);
            SPI_WriteByte((address >> 8) & 0xff);
            SPI_WriteByte(address & 0xff);
            // 读取字体数据
            for (y = 0; y < 24; y++)
            {
                for (x = 0; x < 3; x++)
                {
                    data[y * 3 + x] = SPI_ReadByte();
                }
            }
            FONT_CS_1; // 取消选择字体芯片
            return FONT_SIZE_24x24; // 返回字体大小
            break;

        case FONT_SIZE_32x32: // 处理32x32字体
			  if((MSB == 0xA1)&&((LSB == 0xFD)||(LSB == 0xC1)))
			  {
				  if(LSB == 0xFD)
				  {
					  // 箭头
					  for(y = 0; y < 128; y++)
						data[y] = img2_32[y];
				  }
				  else
				  {
					  // ×
					  for(y = 0; y < 128; y++)
						data[y] = img3_32[y];
					  
				  }
				  break;
			  }
            baseadd = 0x0EDF00; // 基地址
            // 计算地址
            if ((MSB >= 0xA1) && (MSB <= 0xA9) && (LSB >= 0xA1))
            {
                address = (((MSB - 0xA1) * 94 + (LSB - 0xA1)) * 128) + baseadd;
            }
            else if ((MSB >= 0xB0) && (MSB <= 0xF7) && (LSB >= 0xA1))
            {
                address = (((MSB - 0xB0) * 94 + (LSB - 0xA1) + 846) * 128) + baseadd;
            }
            FONT_CS_0; // 选择字体芯片
            SPI_WriteByte(0x03); // 发送命令
            // 发送地址的高、中、低字节
            SPI_WriteByte((address >> 16) & 0xff);
            SPI_WriteByte((address >> 8) & 0xff);
            SPI_WriteByte(address & 0xff);
            // 读取字体数据
            for (y = 0; y < 32; y++)
            {
                for (x = 0; x < 4; x++)
                {
                    data[y * 4 + x] = SPI_ReadByte();
                }
            }
            FONT_CS_1; // 取消选择字体芯片
            return FONT_SIZE_32x32; // 返回字体大小
            break;

        default:
            return 0xFF; // 返回错误代码
            break;
    }
}

// 读取一个字符数据
CharSize Read_One_Char(uint8_t addr, uint8_t *data, CharSize datasize)
{
    uint8_t y, x; // 循环变量
    uint32_t address, baseadd; // 地址变量
    uint8_t ASCIICode = addr; // ASCII码

    switch(datasize) // 根据字符大小选择处理方式
    {
        case CHAR_SIZE_5x7: // 处理5x7字符
            baseadd = 0x1ddf80; // 基地址
            if (ASCIICode >= 0x20 && ASCIICode <= 0x7e) // 确保ASCII码在有效范围内
                address = (ASCIICode - 0x20) * 8 + baseadd; // 计算地址
            FONT_CS_0; // 选择字体芯片
            SPI_WriteByte(0x03); // 发送命令
            // 发送地址的高、中、低字节
            SPI_WriteByte((address >> 16) & 0xff);
            SPI_WriteByte((address >> 8) & 0xff);
            SPI_WriteByte(address & 0xff);
            // 读取字符数据
            for (y = 0; y < 8; y++)
            {
                data[y] = SPI_ReadByte(); // 读取每一行
            }
            FONT_CS_1; // 取消选择字体芯片
            return CHAR_SIZE_5x7; // 返回字符大小
            break;

        case CHAR_SIZE_7x8: // 处理7x8字符
            baseadd = 0x1de280; // 基地址
            if (ASCIICode >= 0x20 && ASCIICode <= 0x7e)
                address = (ASCIICode - 0x20) * 8 + baseadd; // 计算地址
            FONT_CS_0; // 选择字体芯片
            SPI_WriteByte(0x03); // 发送命令
            // 发送地址的高、中、低字节
            SPI_WriteByte((address >> 16) & 0xff);
            SPI_WriteByte((address >> 8) & 0xff);
            SPI_WriteByte(address & 0xff);
            // 读取字符数据
            for (y = 0; y < 8; y++)
            {
                data[y] = SPI_ReadByte(); // 读取每一行
            }
            FONT_CS_1; // 取消选择字体芯片
            return CHAR_SIZE_7x8; // 返回字符大小
            break;

        case CHAR_SIZE_6x12: // 处理6x12字符
            baseadd = 0x1dbe00; // 基地址
            if (ASCIICode >= 0x20 && ASCIICode <= 0x7e)
                address = (ASCIICode - 0x20) * 12 + baseadd; // 计算地址
            FONT_CS_0; // 选择字体芯片
            SPI_WriteByte(0x03); // 发送命令
            // 发送地址的高、中、低字节
            SPI_WriteByte((address >> 16) & 0xff);
            SPI_WriteByte((address >> 8) & 0xff);
            SPI_WriteByte(address & 0xff);
            // 读取字符数据
            for (y = 0; y < 12; y++)
            {
                data[y] = SPI_ReadByte(); // 读取每一行
            }
            FONT_CS_1; // 取消选择字体芯片
            return CHAR_SIZE_6x12; // 返回字符大小
            break;

        case CHAR_SIZE_8x16: // 处理8x16字符
				if((ASCIICode >= 0x30)&&(ASCIICode <= 0x39))
				{
					ASCIICode = ASCIICode - '0';
					for(y = 0;y < 16;y++)
					{
						data[y] = tab_askii[ASCIICode][y];
					}
					break;
				}
//				if((ASCIICode >= 'a')&&(ASCIICode <= 'z'))
//				{
//					ASCIICode = ASCIICode - '0' + 36;
//					for(y = 0;y < 16;y++)
//					{
//						data[y] = tab_askii[ASCIICode][y];
//					}
//					break;
//				}
				if((ASCIICode >= 'A')&&(ASCIICode <= 'Z'))
				{
					ASCIICode = ASCIICode - 'A' + 10;
					for(y = 0;y < 16;y++)
					{
						data[y] = tab_askii[ASCIICode][y];
					}
					break;
				}
            baseadd = 0x1dd780; // 基地址
            if (ASCIICode >= 0x20 && ASCIICode <= 0x7e)
                address = (ASCIICode - 0x20) * 16 + baseadd; // 计算地址
            FONT_CS_0; // 选择字体芯片
            SPI_WriteByte(0x03); // 发送命令
            // 发送地址的高、中、低字节
            SPI_WriteByte((address >> 16) & 0xff);
            SPI_WriteByte((address >> 8) & 0xff);
            SPI_WriteByte(address & 0xff);
            // 读取字符数据
            for (y = 0; y < 16; y++)
            {
                data[y] = SPI_ReadByte(); // 读取每一行
            }
            FONT_CS_1; // 取消选择字体芯片
            return CHAR_SIZE_8x16; // 返回字符大小
            break;
        case CHAR_SIZE_24x48:
        case CHAR_SIZE_12x24: // 处理12x24字符
			  if((ASCIICode >= 0x30)&&(ASCIICode <= 0x39))
				{
					ASCIICode = ASCIICode - '0';
					for(y = 0;y < 48;y++)
					{
						data[y] = tab_askii24[ASCIICode][y];
					}
					break;
				}
				if((ASCIICode >= 'A')&&(ASCIICode <= 'Z'))
				{
					ASCIICode = ASCIICode - 'A' + 10;
					for(y = 0;y < 48;y++)
					{
						data[y] = tab_askii24[ASCIICode][y];
					}
					break;
				}
//				if((ASCIICode >= 'a')&&(ASCIICode <= 'z'))
//				{
//					ASCIICode = ASCIICode - '0' + 36;
//					for(y = 0;y < 48;y++)
//					{
//						data[y] = tab_askii24[ASCIICode][y];
//					}
//					break;
//				}
				if(ASCIICode == '/')
				{
					for(y = 0;y < 48;y++)
					{
						data[y] = tab_askii24[62][y];
					}
					break;
				}
            baseadd = 0x1dff00; // 基地址
            if (ASCIICode >= 0x20 && ASCIICode <= 0x7e)
                address = (ASCIICode - 0x20) * 48 + baseadd; // 计算地址
            FONT_CS_0; // 选择字体芯片
            SPI_WriteByte(0x03); // 发送命令
            // 发送地址的高、中、低字节
            SPI_WriteByte((address >> 16) & 0xff);
            SPI_WriteByte((address >> 8) & 0xff);
            SPI_WriteByte(address & 0xff);
            // 读取字符数据
            for (y = 0; y < 24; y++)
            {
                for (x = 0; x < 2; x++)
                {
                    data[y * 2 + x] = SPI_ReadByte(); // 读取每一行
                }
            }
            FONT_CS_1; // 取消选择字体芯片
            return CHAR_SIZE_12x24; // 返回字符大小
            break;

        case CHAR_SIZE_16x32: // 处理16x32字符
//			  	if((ASCIICode >= 'A')&&(ASCIICode <= 'Z'))
//				{
//					ASCIICode = ASCIICode - 'A';
//					for(y = 0;y < 64;y++)
//					{
//						data[y] = tab_askii32[ASCIICode][y];
//					}
//					break;
//				}
            baseadd = 0x1e5a50; // 基地址
            if (ASCIICode >= 0x20 && ASCIICode <= 0x7e)
                address = (ASCIICode - 0x20) * 64 + baseadd; // 计算地址
            FONT_CS_0; // 选择字体芯片
            SPI_WriteByte(0x03); // 发送命令
            // 发送地址的高、中、低字节
            SPI_WriteByte((address >> 16) & 0xff);
            SPI_WriteByte((address >> 8) & 0xff);
            SPI_WriteByte(address & 0xff);
            // 读取字符数据
            for (y = 0; y < 32; y++)
            {
                for (x = 0; x < 2; x++)
                {
                    data[y * 2 + x] = SPI_ReadByte(); // 读取每一行
                }
            }
            FONT_CS_1; // 取消选择字体芯片
            return CHAR_SIZE_16x32; // 返回字符大小
            break;

        default: 
            return 0xFF; // 返回错误代码
            break;
    }
}
