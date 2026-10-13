# ADC 采集踩坑记录（STM32F103 + HAL 库）

> 日期：2026-10-13
> 硬件：STM32F103，ADC1 双通道——PC0（充电/电源，ADC1_IN10）、PC1（电池，ADC1_IN11），各接 33K/33K 分压
> 现象级别：数据全错但无任何报错（err=0），极具迷惑性

---

## 一、故障现象（按发现顺序）

1. 两通道读数恒为 0：`[ADC] PC0=0 mV | PC1=0 mV | err=0`
2. 万用表实测**其中一个引脚有电压**，ADC 仍读 0
3. 读数变成"两通道数值永远相同"：调整任何一个通道的电压，**另一个通道的读数跟着一起变**，就像两个通道都在采同一个脚

## 二、排查过程

| 步骤 | 手段 | 结论 |
|---|---|---|
| 1 | 核对 `.ioc` / `adc.c` / `main.h` / `power_adc.c` 四处配置 | 通道号、引脚、采样时间全部一致，配置无错 |
| 2 | 看错误码 | `err=0`：Start、EOC 等待、读 DR 全部"成功"——**无报错不代表数据有效** |
| 3 | 打印原始码值 raw（不只是 mV） | 排除"折算公式错"，确实是采样结果为 0 |
| 4 | 加 VREFINT 自检（打印内部 1.2V 基准折算的 vdda） | 用于判别 ADC 内核好坏（内部通道不受外部接线影响） |
| 5 | 加"逐通道独立采样 vs 扫描配对采样"对照打印 | 用户观察到两路数值始终同步 → 锁定"串读" |

## 三、根因（铁证）

**STM32F1 HAL 库：扫描模式（ScanConvMode = ENABLE 且 NbrOfConversion >= 2）下，EOC 标志只在整个转换序列结束时置位，不再逐通道置位。**

源码证据（`Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc.c`）：

```c
/* hal_adc.c:1237 HAL_ADC_PollForConversion 注释原文：
 * "If sequence conversion for regular group (scan mode enabled and
 *  NbrOfConversion >=2), flag EOC is set only at the end of the sequence."
 * 即：轮询被替换成"等待固定 CPU 周期"，不再检查 EOC */

/* hal_adc.c:1307 —— 轮询函数每次都会把 EOC 清掉：
 * __HAL_ADC_CLEAR_FLAG(hadc, ADC_FLAG_STRT | ADC_FLAG_EOC); */
```

于是原来的"扫描配对读法"（一次 Start 后连读两次 EOC，分别取 rank1/rank2）实际发生的是：

```
Start → 序列(2 个转换)跑完 → DR = rank2
PollForConversion(第1次)  → 固定延时后返回 OK（根本没看 EOC），并把 EOC 清了
GetValue(第1次)           → DR = rank2        ← 第一次就读的是最后一个通道
PollForConversion(第2次)  → 又延时、又清 EOC、返回 OK
GetValue(第2次)           → DR = rank2（没有新转换，DR 没变）
```

**两次读到的都是序列最后一个通道（rank2 = PC1）**，rank1（PC0）从未被读出。这完美解释全部症状：

- 两通道数值永远相同、动一个另一个跟着变（DR 里只有一个值）；
- PC0 有电压但读 0（PC0 是 rank1，没被读过）；
- `err=0`（固定延时等待在逻辑上"永远不超时"）。

## 四、修复

`power_adc.c` 的 `PowerAdc_Init()` 里**强制改回单通道模式**（覆盖 CubeMX 生成的扫描配置）：

```c
/* F1 扫描模式(>=2 个转换)下 EOC 只在整个序列结束置位，逐通道轮询不可靠 */
hadc1.Init.ScanConvMode    = ADC_SCAN_DISABLE;
hadc1.Init.NbrOfConversion = 1u;
if (HAL_ADC_Init(&hadc1) != HAL_OK) return 1u;
```

读数改为每次读之前 `HAL_ADC_ConfigChannel()` 切换通道（rank1）+ 单次转换 + 真实 EOC 轮询。
单通道模式下 EOC 每次转换都置位，轮询语义正确，两通道数据天然独立。

相关开关：`power_adc.h` 的 `PWR_ADC_SEQ_SCAN = 0`（并写明原因注释，防止以后被"改回与 CubeMX 一致"）。

## 五、教训与检查清单

1. **F1 的 ADC 扫描模式 + 逐次 EOC 轮询是死路**。要多通道数据只有两条正路：
   - 单通道模式 + 软件切通道（本项目采用，简单可靠）；
   - 扫描模式 + **DMA 搬运**（CubeMX 常规做法，需配 DMA 通道）。
2. **CubeMX 面板配置 ≠ 驱动可用**：CubeMX 生成"扫描双通道 + 轮询读取"的组合在 F1 上本身就有坑，生成后要按上面二选一改造。
3. **`err=0` 不代表数据有效**：固定延时式轮询把错误也"吞"了。调试时要看原始码值，不能只看折算值。
4. **配置一致性要四处核对**：`.ioc`、CubeMX 生成的 `adc.c`、引脚宏 `main.h`、驱动里的模式宏（本工程还出现过 `PWR_ADC_SEQ_SCAN` 头文件/源文件默认值打架的情况）。
5. **排障三板斧**（本次全部用到，值得复用）：
   - 打印 **raw 原始码值**而不是只打印换算结果；
   - 用 **VREFINT（内部 1.2V 基准）自检 ADC 内核**——内部通道与外部接线无关，能一刀切开"内核坏 / 外部没信号"；
   - **逐通道独立采样 vs 批量采样对照**，快速区分"软件串读"和"硬件等电位"。
6. **软件结论要等硬件测量闭环**：万用表量引脚电压 + ADC 读数 + 打印三者对齐才能定案；本次"引脚有电压但读 0"最终指向软件读法，而非最初怀疑的接线。

## 六、关键代码位置

| 文件 | 位置 | 内容 |
|---|---|---|
| `Core/Src/power_adc.c` | `PowerAdc_Init()` | 强制单通道模式 + 校准 |
| `Core/Src/power_adc.c` | `PowerAdc_SetChannel()` / `PowerAdc_ReadRaw()` | 切通道 + 多次采样去极值平均 |
| `Core/Inc/power_adc.h` | `PWR_ADC_SEQ_SCAN` | 模式开关（含 F1 限制原因注释） |
| `Core/Src/main.c` | `Ui_Update()` | 每 2s 打印 raw/mV/vdda/err 调试行 |
| `Drivers/.../stm32f1xx_hal_adc.c` | `HAL_ADC_PollForConversion` 注释与 1307 行 | 根因出处 |
