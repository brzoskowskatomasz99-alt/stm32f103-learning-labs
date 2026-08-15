# LLCC68 Driver 导入记录（UPSTREAM）

## 本地来源路径
- E:\llcc68_driver-2.5.0（用户提供的本地目录，根目录直接含 CHANGELOG.md、LICENSE.txt、README.md 与 src/）

## 版本核验（从内容核验，非仅目录名）
- 版本确认为 **2.5.0**：
  - `src/llcc68_driver_version.h` 第 60 行：`#define LLCC68_DRIVER_VERSION "v2.5.0"`
  - `CHANGELOG.md` 第 7 行：`## [2.5.0] - 2025-06-16`，其条目内容（`llcc68_tx_modulation_workaround` 公开化；版本信息仅保留 `LLCC68_DRIVER_VERSION` 宏与 `llcc68_driver_version_get_version_string` 函数）与已读取的 `llcc68.h`/`llcc68_driver_version.c` 实际内容一致。
- 提交哈希：本地目录未保留 Git 元数据，**本地压缩包未提供可验证提交哈希**。

## 导入日期
- 2026-08-06

## 导入文件列表（10 个，均为原样复制）
| 来源（E:\llcc68_driver-2.5.0\） | 目标（E:\TEMPLATE\Template\Drivers\LLCC68\） |
|---|---|
| src/llcc68.h | Inc/llcc68.h |
| src/llcc68_driver_version.h | Inc/llcc68_driver_version.h |
| src/llcc68_hal.h | Inc/llcc68_hal.h |
| src/llcc68_regs.h | Inc/llcc68_regs.h |
| src/llcc68_status.h | Inc/llcc68_status.h |
| src/llcc68.c | Src/llcc68.c |
| src/llcc68_driver_version.c | Src/llcc68_driver_version.c |
| LICENSE.txt | LICENSE.txt |
| README.md | README.md |
| CHANGELOG.md | CHANGELOG.md |

## SHA-256 一致性核对
- 已完成：上述 10 个文件逐对（来源 vs 目标）SHA-256 一致（MATCH）。
- 示例：
  - llcc68.c：A18078446AF583774722A69393731ABCCB67BC1873F68F2A321D3C41E62612D9
  - llcc68_hal.h：261F83D9ABEA60E2A2477420FFD9F5E21935F98D6ED53AB799877E6E2A4F4548
  - llcc68_driver_version.h：AF6EF0266663238B9BF98C32EDFBC038E35CC3958B0ABDEC09C5FBC808EB1780

## 上游文件未经修改
- 复制后的上游驱动源码、头文件、许可证与说明文件均未做任何编辑（"原样导入"）。平台适配层（Core/Inc/llcc68_hal_stm32.h、Core/Src/llcc68_hal_stm32.c）为本项目新建文件，不属于上游文件。
