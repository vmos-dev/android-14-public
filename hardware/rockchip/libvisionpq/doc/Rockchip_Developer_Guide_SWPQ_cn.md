# RK SW(SoftWare) PQ SDK 使用指南

发布版本：v1.0.2

发布日期：2022-11-28

文件密级：□绝密   □秘密   ■内部资料   □公开

---

**免责声明**

本文档按“现状”提供，瑞芯微电子股份有限公司（“本公司”，下同）不对本文档的任何陈述、信息和内容的准确性、可靠性、完整性、适销性、特定目的性和非侵权性提供任何明示或暗示的声明或保证。本文档仅作为使用指导的参考。

由于产品版本升级或其他原因，本文档将可能在未经任何通知的情况下，不定期进行更新或修改。

**商标声明**

“Rockchip”、“瑞芯微”、“瑞芯”均为本公司的注册商标，归本公司所有。

本文档可能提及的其他所有注册商标或商标，由其各自拥有者所有。

**版权所有** **© 2022 瑞芯微电子股份有限公司**

超越合理使用范畴，非经本公司书面许可，任何单位和个人不得擅自摘抄、复制本文档内容的部分或全部，并不得以任何形式传播。

瑞芯微电子股份有限公司

Rockchip Electronics Co., Ltd.

地址：     福建省福州市铜盘路软件园A区18号

网址：     [www.rock-chips.com](http://www.rock-chips.com)

客户服务电话： +86-4007-700-590

客户服务传真： +86-591-83951833

客户服务邮箱： [fae@rock-chips.com](mailto:fae@rock-chips.com)

---

**读者对象**

本文档主要适用于以下工程师：

- **Rockchip内部**技术支持工程师
- **Rockchip内部**软件开发工程师

**修订记录**

| **日期**   | **版本** | **适配SDK版本** |  **作者** | **审核**           | **修改说明**                    |
|:----------:|:--------:|:--------------:|:----------:|:-------:|:---------------------------------------- |
| 2022/08/18 | 1.0.0    | librkswpq_v6.0.0 (pre) | 吴方熠 | 夏海军 | 初始版本                                 |
| 2022/11/28 | 1.0.1    | librkswpq_v8.0.0 (pre) | 吴方熠 | 夏海军 | 增加支持图像格式和ZME模块，增加计算精度和性能 |
| 2023/04/xx | 1.1.0    | librkswpq v0.1.0 | 吴方熠 |       | 基于 pipeline 框架修改为通用版本              |

## 目 录

[TOC]

- [RK SW(SoftWare) PQ SDK 使用指南](#rk-swsoftware-pq-sdk-使用指南)
  - [目 录](#目-录)
  - [概述](#概述)
    - [Feature支持](#feature支持)
    - [特殊硬件需求](#特殊硬件需求)
    - [已明确支持平台及性能数据](#已明确支持平台及性能数据)
    - [各模块理论带宽和占用率数据](#各模块理论带宽和占用率数据)
    - [各模块支持的输入/输出图像格式](#各模块支持的输入输出图像格式)
  - [SDK版本说明](#sdk版本说明)
    - [版本号格式与递增规则](#版本号格式与递增规则)
      - [SDK版本号格式与递增规则](#sdk版本号格式与递增规则)
    - [版本号查询](#版本号查询)
      - [strings命令查询](#strings命令查询)
      - [日志打印](#日志打印)
      - [函数接口查询](#函数接口查询)
  - [数据结构说明](#数据结构说明)
    - [rkpq\_query\_cmd](#rkpq_query_cmd)
    - [rkpq\_imgbuf\_info](#rkpq_imgbuf_info)
      - [基本概念](#基本概念)
      - [辅助接口的使用 - rkpq\_query() with RKPQ\_QUERY\_IMG\_BUF\_INFO](#辅助接口的使用---rkpq_query-with-rkpq_query_img_buf_info)
      - [其他说明](#其他说明)
    - [rkpq\_init\_params](#rkpq_init_params)
    - [rkpq\_pipe\_fmt\_info 和 rkpq\_pipe\_res\_info](#rkpq_pipe_fmt_info-和-rkpq_pipe_res_info)
  - [应用接口说明](#应用接口说明)
    - [rkpq\_init()](#rkpq_init)
    - [rkpq\_proc()](#rkpq_proc)
    - [rkpq\_deinit()](#rkpq_deinit)
    - [rkpq\_query()](#rkpq_query)
    - [rkpq\_set\_loglevel()](#rkpq_set_loglevel)
    - [rkpq\_set\_cache\_path()](#rkpq_set_cache_path)
    - [rkpq\_set\_default\_cfg()](#rkpq_set_default_cfg)
    - [rkpq\_get\_default\_cfg()](#rkpq_get_default_cfg)
    - [rkpq\_set\_inputs() \& rkpq\_set\_outputs()](#rkpq_set_inputs--rkpq_set_outputs)
    - [rkpq\_set\_target\_platform](#rkpq_set_target_platform)
  - [调用方法和可执行程序使用介绍](#调用方法和可执行程序使用介绍)
    - [集成调用示例](#集成调用示例)
    - [可执行程序使用介绍](#可执行程序使用介绍)
  - [debug \& tuning 工具](#debug--tuning-工具)
    - [通过PQTool进行tuning](#通过pqtool进行tuning)
    - [通过adb properties进行tuning](#通过adb-properties进行tuning)
    - [设置dump行为](#设置dump行为)
  - [疑难解答](#疑难解答)
    - [错误码](#错误码)
    - [常见错误分析解答](#常见错误分析解答)

## 概述

SWPQ (SoftWare Picture Quality) 主要功能是利用软件对输出画质进行进一步改善，是现有各平台图像处理硬件 (RGA、IEP、VOP) 的补充。

目前 SWPQ 中大部分模块主要运行于在**GPU**和**NPU**设备上，部分模块（如：DM, SD, AISR, FE）仅支持在**NPU**设备上运行。

### Feature支持

- 数据格式
  - 输入格式支持：
    - RGB888 / BGR888 / RGBA8888 / BGRA8888 / RGB565 / BGR565
    - YUV444SP_NV24 / YUV422SP_NV16 / YUV420SP_NV12 / YUV420SP_NV15 / YUV420SP_NV30
  - 输出格式支持：
    - RGB888 / BGR888 / RGBA8888 / BGRA8888 / RGB565 / BGR565
    - YUV444SP_NV24 / YUV422SP_NV16 / YUV420SP_NV12 / YUV420SP_NV15 / YUV420SP_NV20 / YUV420SP_NV30
  - 分辨率支持：
    - 理论最低: 128x128
    - 理论最高: 16384x16384, 高于 8K(7680x4320) 分辨率暂未经过验证
  - 当前版本**只支持Non-FBC格式**，FBC格式目前不支持

- 色域变换模块(Corlor Space Convert, CSC)
  - 支持 Y2R, Y2Y, R2Y, R2R
  - 支持 YCbCr BT.601 / BT.709 完整特性
  - 支持 Full-Range / Limited-Range 相互转换
  - 支持调整亮度、色调、对比度、饱和度、色温和颜色增益

<!-- - 图像格式转换模块(Format Convert, CVT)
  - 主要用于同类图像格式下不同通道间的转换(Y2Y/R2R)，如NV12到NV24, RGBA到RGB等
  - 内部模块，无需特别指定参数 -->

- 动态对比度模块(Dynamic Contrast Improvement, DCI)
  - 支持全局动态对比度调整
  - 支持调整亮区/暗区对比度增益

- 自动色彩管理模块(Auto Color Management, ACM)
  - 支持自动调整亮度、色调、饱和度
  - 支持特定亮度、色调、饱和度改变

- 缩放管理引擎模块(Zoom Manage Engine, ZME)
  - 支持任意倍率缩放
  - 支持deringing功能

- 锐化(Sharpen, SHP)
  - 支持全局清晰度调整
  - 支持调整 coring / gain / limit 等相关配置

- AI超分辨率模块(AI Super Resolution, AISR)
  - **仅支持2倍率放大**
  - 方向滤波 + 双三次插值, 支持任意分辨率输入 （**暂不支持**）
  - 支持基于RKNN v1.4的AISR （**当前仅支持此部分功能**）
    - **支持RK3588平台，1080P => 4K**
    - **支持RK356X平台，540P => 1080P**

- AI场景检测模块(AI Scene Detection, AISD)
  - AI模块，仅支持RK3588平台(RKNN版本需大于等于1.4)
  - 目前支持场景分类: 自然 / 文字
  - 目前主要辅助其他模块(AISR)搭配使用

- AI去噪模块(AI DeMosquito, AIDM)
  - AI模块，仅支持**分辨率为1080P**的YUV图像输入输出，且**仅支持RK356X平台**(RKNN版本需大于等于1.4)
  - 去蚊虫噪声, 主要用于处理压缩率比较高的图像，或码率较低的视频
  - 应用项目：云办公场景等

- AI去伪色模块(AI DeFalseColor, AIDFC）
  - AI模块，RKNN版本需大于等于1.5
  - YUV420->YUV444

- AI文档检测模块(AI Document Detection, AIDD)
  - AI模块，RKNN版本需大于等于1.5
  - 检测图像内的文档区域，得到四个顶点

- AI文档增强模块(AI Document Enhancement, AIDE)
  - AI模块，RKNN版本需大于等于1.5
  - 将带有四个顶点信息的文档图像变换为正投影，并进行增强

- AI人脸增强混合模块(Mixture Face Enhancement, MixFE)
  - AI混合模块，RKNN版本需大于等于1.5
  - 人脸检测+增强+4x放大

- AI视频插帧混合模块(MixMSSR)
  - AI混合模块，RKNN版本需大于等于1.5
  - 支持MEMC， 24/25/30 fps => 60fps



### 特殊硬件需求

| **特殊硬件**   | **说明**                  |
| --- | ------------------------------------ |
| GPU | 绝大部分模块运行于GPU，需求OpenCL 2.0及以上 |
| NPU | AISR/AISD等模块需NPU硬件，RKNN 1.4及以上版本，部分AI模块需要1.5以上版本 |


### 已明确支持平台及性能数据

以下数据基于YUV444SP_NV24格式
<table>
   <tr align="center">
      <td rowspan="2"><b>平台</b></td>
      <td rowspan="2"><b>GPU型号与频率</b></td>
      <td rowspan="2"><b>输入分辨率</b></td>
      <td rowspan="1" colspan="8"><b>各模块平均效率</b>(高性能模式)[ms]</td>
   </tr>
   <tr align="center">
      <td><b>CSC</b></td>
      <td><b>DCI</b></td>
      <td><b>ACM</b></td>
      <td><b>SHP</b></td>
      <td><b>ZME</b></td>
      <td><b>AISR</b></td>
      <td><b>AIDM</b></td>
      <td><b>AIFE</b></td>
   </tr>

   <tr align="center">
      <td>RK3399</td>
      <td>Mali-T860 MP4 @800MHz</td>
      <td>1920x1080</td>
      <td>5.5</td>
      <td>4+3</td>
      <td>x</td>
      <td>x</td>
      <td>45 (2x, only luma)</td>
      <td>x</td>
      <td>x</td>
      <td>x</td>
   </tr>

   <tr align="center">
      <td rowspan="2">RK3568</td>
      <td rowspan="2">Mali-G52 @800MHz</td>
      <td>960x540</td>
      <td>3</td>
      <td>16+2</td>
      <td>3.5</td>
      <td>3.5 (lite)</td>
      <td>24 (2x, only luma)</td>
      <td>25 (2x, only luma)</td>
      <td>x</td>
      <td>x</td>
   </tr>

   <tr align="center">
      <td>1920x1080</td>
      <td>7</td>
      <td>8+5</td>
      <td>7</td>
      <td>10 (lite)</td>
      <td>65 (2x, only luma)</td>
      <td>x</td>
      <td>30</td>
      <td>x</td>
   </tr>

   <tr align="center">
      <td rowspan="2">RK3588</td>
      <td rowspan="2">Mali-G610 MC4 @1000MHz</td>
      <td>1920x1080</td>
      <td>1.6</td>
      <td>1.2</td>
      <td>1.3</td>
      <td>1.2</td>
      <td>5.5 (2x, only luma)</td>
      <td>17 (2x, only luma)</td>
      <td>x</td>
      <td>x</td>
   </tr>

   <tr align="center">
      <td>3840x2160</td>
      <td>4</td>
      <td>3.5</td>
      <td>4</td>
      <td>4.2</td>
      <td>TBD</td>
      <td>TBD</td>
      <td>x</td>
      <td><font color= "#FF0000"> 30 (960x540 => 4K, async mode)</font><br></td>
  </tr>

  <tr align="center">
      <td rowspan="2">RK3576</td>
      <td rowspan="2">Mali-G52 MC4EE2 @800MHz</td>
      <td>1920x1080</td>
      <td>IP support</td>
      <td>IP support</td>
      <td>IP support</td>
      <td>IP support</td>
      <td>IP (Y/UV separated)</td>
      <td>30</td>
      <td>TBD</td>
      <td>TBD</td>
   </tr>

   <tr align="center">
      <td>3840x2160</td>
      <td>IP support</td>
      <td>IP support</td>
      <td>IP support</td>
      <td>IP support</td>
      <td>N/A</td>
      <td>TBD</td>
      <td>TBD</td>
      <td>TBD</td>
  </tr>

</table>

注：
1. 表中符号`x`表示相应分辨率情况下对应的模块不支持；
2. 表中符号`TBD`表示未经过测试，数据缺失；
3. 表中`lite`注释表示为简化后的算法模块

### 各模块理论带宽和占用率数据
| 模块 | GPU占用率[%] | GPU带宽[MB/帧] | NPU带宽[MB/帧] | 总带宽[MB/帧] | 总带宽备注 |
|:----:|:-----------:|:---------------|:---------------|:-------------| ---- |
| CSC  | ~30% @60fps on RK3568 | 9MB   | \      | 9MB   | 1080P NV12 => 1080P NV12 |
| DCI  | ~54% @60fps on RK3568 | 6MB   | \      | 6MB   | 1080P NV12 => 1080P NV12 |
| ACM  | ~32% @60fps on RK3568 | 6MB   | \      | 6MB   | 1080P NV12 => 1080P NV12 |
| SHP  | ~50% @60fps on RK3568 | 38MB  | \      | 38MB  | 1080P NV12 => 1080P NV12 |
| ZME  | ~30% @60fps on RK3588 | 150MB | \      | 150MB | 1080P NV12 => 4K NV12 |
| AISR | ~40% @30fps on RK3588 | 70MB  | 180MB  | 250MB | 1080P NV12 => 4K NV12 |
| AISD |       暂无数据        | 2MB   | 2MB    | 4MB   | 1080P NV12 => 1080P NV12 |
| AIDM | ~32% @30fps on RK3568 | 36MB  | 15MB   | 51MB  | 1080P NV12 => 1080P NV12 |
| AIDFC| TBD | TBD | TBD | TBD | TBD |
| AIDE | TBD | TBD | TBD | TBD | TBD |
| AIDD | TBD | TBD | TBD | TBD | TBD |
| MixFE | ~35% @30fps on RK3588 |       | ~430MB |       | 540P  NV12 => 4K NV12; 异步模式，延迟1帧 |
| MixMSSR | TBD | TBD | TBD | TBD | TBD |

注：
1. 带宽数据根据右侧备注中输入输出图像和格式计算得到

### 各模块支持的输入/输出图像格式
| 模块 | 支持的输入/输出格式 | 备注 |
|:----:|:--------------|:---------|
| CSC  | 输入: YUV4xxSP_8bit, YUV444I_VU24, API中所有RGB系列 <br> 输出: YUV4xxSP_8bit, YUV444I_VU24, API中所有RGB系列 | 一般情况下输出格式或色彩空间和输入不同 |
| DCI  | 输入: YUV4xxSP_8bit, YUV444I_VU24, Y8 <br> 输出: YUV4xxSP_8bit, YUV444I_VU24, Y8 | 输出格式和输入相同，只处理Y通道 |
| ACM  | 输入: YUV4xxSP_8bit, YUV444I_VU24 <br> 输出: YUV4xxSP_8bit, YUV444I_VU24 | 输出格式和输入相同 |
| SHP  | 输入: YUV4xxSP_8bit, YUV444I_VU24, Y8 <br> 输出: YUV4xxSP_8bit, YUV444I_VU24, Y8 | 输出格式和输入相同，只处理Y通道，full-range |
| ZME  | 输入: YUV4xxSP_8bit, YUV444I_VU24, API中所有RGB系列, Y8, UV8 <br> 输出: YUV4xxSP_8bit, YUV444I_VU24, API中所有RGB系列, Y8, UV8 | 一般不改变输入格式，注5 |
| AISR | 输入: YUV4xxSP_8bit, YUV444I_VU24, Y8 <br> 输出: YUV4xxSP_8bit, YUV444I_VU24, Y8 | 一般不改变输入格式，注5 |
| AISD | 输入: YUV4xxSP_8bit, YUV444I_VU24, API中所有RGB系列 <br> 输出: 非图像输出 | 无图像输出 |
| AIDM | 输入: YUV4xxSP_8bit, Y8 <br> 输出: YUV4xxSP_8bit, Y8 | 输出格式和输入相同，只处理Y通道 |
| AIDFC| 输入: YUV420SP_NV12 <br> 输出: YUV444SP_NV24, RGBA8888 |  |
| AIDE | 输入: RGB888 <br> 输出: RGB888 | |
| AIDD | 输入: YUV420SP_NV12, RGB[A8]888 <br> 输出: YUV420SP_NV12 | |
| MixSHPACM | 输入: YUV420SP_NV12, RGBA8888 <br> 输出: YUV420SP_NV12 | |
| MixFE | 输入: TBD <br> 输出: TBD | TBD |
| MixMSSR | 输入: TBD <br> 输出: TBD | TBD |

注：
1. `YUV4xxSP_8bit`表示`YUV420SP_NV12, YUV422SP_NV16, YUV444SP_NV24`均支持。
2. `API中所有RGB系列`表示`RGBA8888, RGB888, RGB565`以及对应BGR通道顺序的格式。
3. 以上表中所有`RGB`系列格式均包含了其对应的`BGR`通道顺序的格式。
4. `RGB565`等`packed`格式的图像格式，通道排列顺序遵循`Most Significant Byte(MSB)`优先原则。
5. 由于`ZME`/`AISR`模块对亮度和色度模块的缩放是分离执行的，故在做缩放的同时也支持`YUV4xxSP_8bit`表示的格式之间相互转换，但不能转变色彩空间。
6. 除`CSC`模块是明确转换色彩空间的功能，输出无论是**Full-Range**还是**Limited-Range**，都可以保证其取值范围是正确的。但其他模块默认输入输出的色彩空间是**Full-Range**，故强烈建议对**Limited-Range**的输入图像过一次`CSC`模块先转到**Full-Range**，再进行后续模块的处理; **Limited-Range**的输出对这些模块来说不能保证还是**Limited-Range**。
7. 大部分**非AI模块**基本只支持**YUV Full-Range**输入，大部分**AI模块**还支持**RGB Full-Range**输入。



### AI 模块对应模型文件
| 模块 | 模型文件 | md5值 |
|:----|:-------|:----|
| AISR  | rkaipq_sr_model_rknn140_rk356x_540p.rknn <br> rkaipq_sr_model0_rknn140_rk3588_1080p.rknn <br> rkaipq_sr_model1_rknn140_rk3588_1080p.rknn | 080d8cc840f38a7ed3602bdc0a9acc6a <br> e0bd9da45fd5c598b11d46731308c0eb <br> ba3b4a372ef72cd365bf377f80016d52 |
| AISD  | rkaipq_sd_model_rknn140_rk3588.rknn | abfd6d291e0cb017c2306b565e432c8f |
| AIDM  | rkaipq_dm_model_rknn140_rk356x.rknn | afcfdc7b24f88390b24eb5ba57061b82 |
| AIDFC | rkaipq_dfc_model_rknn150_rk3588.rknn | 3cb6e9905915af4965693e84f16ec973 |
| AIDE  | rkaipq_de_model_rknn150_rk3588.rknn <br> rkaipq_de_model_rknn162_rk3576.rknn |b6a1dd05b0df34b788f55cc968bbd71f <br> 416afaa26c159809a5475d5f43f98e6e |
| AIDD  | rkaipq_dd_model0_rknn150_rk3588.rknn <br> rkaipq_dd_model1_rknn150_rk3588.rknn | 5d33642ab5cd2346eb4aa7b782c16977 <br> 257945db4dc3197e188ed0e9e69f7598 |
| MixFE |  |  |
| MixFE2 |  |  |
| MixFE3 |  |  |
| MixFE4 |  |  |
| MixFE4 |  |  |
| MixMSSR | rkaipq_mssr_model0_IFBlockX4Stage0_rknn162_rk3576.rknn <br> rkaipq_mssr_model0_IFBlockX4Stage1_rknn162_rk3576.rknn <br> rkaipq_mssr_model0_IFBlockX4Stage2_rknn162_rk3576.rknn <br> rkaipq_mssr_model0_IFBlockX5Stage0_rknn162_rk3576.rknn <br> rkaipq_mssr_model0_IFBlockX5Stage1_rknn162_rk3576.rknn <br> rkaipq_mssr_model0_IFBlockX5Stage2_rknn162_rk3576.rknn <br> rkaipq_mssr_model0_NaturalSR540to1080_rknn162_rk3576.rknn <br> rkaipq_mssr_model0_NaturalSR1080to4K_rknn162_rk3576.rknn <br> rkaipq_mssr_model0_EbookSR480to960_rknn162_rk3576.rknn <br> rkaipq_mssr_model0_rd_rknn162_rk3576.rknn <br> rkaipq_mssr_model0_sd_rknn162_rk3576.rknn | 666d32453a82120735a238f2b2f723b0 <br> eb13784d88fd0b6788774b3832b9e614 <br> af98b60b1394df8e0b75943785d440a3 <br> 71ba442eb493c4a84c89201f6c1daa8a <br> 620161e6dd71ccb1e32d0241d268d12b <br> 4b2a3bafe215f816ac1251f463374ded <br> 70caba7674a7bb149102cbddb69e673e <br> b424c8159331ac993f555b7be32ff0ea <br> 9b435ba3603b06b909fc35ecc02c34fe <br> 315a865fbb8c0447fe3fe6bd7e0ddca0 <br> 8bf9787b62242f79ebd5904e80cd378f|

注：
以上模型文件为算法库内部文件，对应的发布文件后缀名为`.bin`，md5值与表内不一致。

# 应用接口说明


## SDK版本说明

### 版本号格式与递增规则

#### SDK版本号格式与递增规则
版本号格式:
```
RKSWPQ_major.minor.revision.build.time
```

版本号递增规则:

| 名称     | 规则                                            | 备注 |
| -------- | ----------------------------------------------- | ---- |
| major    | 主版本号，当提交不向下兼容的API修改。             | - |
| minor    | 次版本号，当向下兼容的功能性新增或删除。          | major更新后重置 |
| revision | 修订版本号，当向下兼容的功能补充或致命的问题修正。 | major/minor更新后重置 |
| build    | 编译版本号，当向下兼容的问题修正。                | 一直递增，不重置 |
| time     | 编译时间。                                      | - |


### 版本号查询

#### strings命令查询

以Android R 64位为例：

```shell
:/# strings /vendor/lib64/librkswpq.so | grep RKSWPQ_
RKSWPQ_0.0.4.2986.202401171019
```

#### 日志打印

当调用`rkpq_init()`时，会打印版本号。

```txt
--s-- rkpq_init()
--------------------------------
        Version: RKSWPQ_0.0.4.2986.202401171019 (CPU-64bit)
```

当调用`rkpq_proc()`时，也会打印版本号。
```txt
--s-- rkpq_proc() #0 ---- RKSWPQ_0.0.4.2986.202401171019
```

#### 函数接口查询

调用`rkpq_query()`接口查询版本号，版本信息位于结构体`rkpq_version_info`中。具体使用说明可以查看 **应用接口说明** 章节。

```c
rkpq_version_info version_info;
rkpq_query(context, RKPQ_QUERY_SDK_VERSION, sizeof(rkpq_version_info), &version_info);
printf("SDK version: %d.%d.%d, %s\n", verInfo.nVerMajor, verInfo.nVerMinor, verInfo.nVerRvson, verInfo.sVerInfo);
// 输出内容: SDK version: 0.0.4, RKSWPQ_0.0.4.2986.202401171019
```


## 数据结构说明

下面罗列出的数据结构较为重要或复杂，文中未罗列出的数据结构请查看`rkpq_api.h`头文件相关注释。

注：头文件中带有`reserved`备注字样的属性or条目暂未支持。

### rkpq_query_cmd
```c
/* rkpq_query() 接口所支持的相关信息查询条目列表 */
typedef enum _rkpq_query_cmd
{
    RKPQ_QUERY_SDK_VERSION = 0,         /* 用于查询 SDK 版本信息 */
    RKPQ_QUERY_PERF_INFO,               /* 用于查询运行耗时信息 */
    RKPQ_QUERY_MODULES_SUPPORT,         /* 用于查询 SDK 在该版本下支持的模块 */
    RKPQ_QUERY_MODULES_ROI_SUPPORT,     /* 用于查询支持ROI操作的模块 */
    RKPQ_QUERY_IMG_RES_CHANGE_SUPPORT,  /* 用于查询是否支持运行时改变图像分辨率 */
    RKPQ_QUERY_IMG_FMT_CHANGE_SUPPORT,  /* 用于查询是否支持运行时改变图像格式 */
    RKPQ_QUERY_IMG_FMT_INPUT_SUPPORT,   /* 用于查询支持的输入图像格式 */
    RKPQ_QUERY_IMG_FMT_OUTPUT_SUPPORT,  /* 用于查询支持的输出图像格式 */
    RKPQ_QUERY_IMG_COLOR_SPACE_SUPPORT, /* 用于查询支持的图像色彩空间 */
    RKPQ_QUERY_IMG_BUF_INFO,            /* 用于查询图像的内存对象信息，查询前必须要先指定图像格式和尺寸等信息，具体见 rkpq_imgbuf_info */
    RKPQ_QUERY_IMG_ALIGNMENT_OCL,       /* 用于查询OpenCL图像在宽度上的对齐量，单位：pixel */
    RKPQ_QUERY_BUF_ALIGNMENT_OCL,       /* 用于查询OpenCL子图像需要的偏移(offset)的对齐量，单位：byte */
    RKPQ_QUERY_RKNN_SUPPORT,            /* 用于查询是否支持RKNN模型在NPU上运行 */
    RKPQ_QUERY_MAX,                     /* 条目数量标志，无实际意义 */
} rkpq_query_cmd;
```

### rkpq_imgbuf_info
```c
/**
 * 定义了输入/输出图像的格式与尺寸等相关信息，非常重要！
 *  - 该结构体指定了输入/输出图像的基本格式信息内存信息
 *  - 可使用'RKPQ_QUERY_IMG_BUF_INFO'条目自动计算结构体成员变量的取值
 *  - 需保证该结构体的取值与实际输入/输出的图像buffer的虚宽、虚高、大小、排列方式等保持一致！
 */
typedef struct _rkpq_imgbuf_info
{
    uint32_t    nColorSpace;                        /* [i] 色彩空间，见 rkpq_clr_spc */
    uint32_t    nPixFmt;                            /* [i] 图像格式，见 rkpq_img_fmt */
    uint32_t    nDrmFmt;                            /* [i/o] reserved */
    uint32_t    nPixWid;                            /* [i] 图像宽度，单位：pixel，建议以4对齐 */
    uint32_t    nPixHgt;                            /* [i] 图像高度，单位：pixel，建议以2对齐 */
    uint32_t    nEleDepth;                          /* [i] 像素第一个元素（YUV的Y，RGB的R）的深度(bpc), 单位：bit */
    uint32_t    nAlignment;                         /* [i] buffer的行对齐量, 单位：byte */
    uint32_t    aWidStrides[RKPQ_MAX_PLANE_NUM];    /* [i/o] 每个平面的虚宽, 单位：byte，建议以64 byte对齐 */
    uint32_t    aHgtStrides[RKPQ_MAX_PLANE_NUM];    /* [i/o] 每个平面的虚高, 单位：pixel */
    size_t      aPlaneSizes[RKPQ_MAX_PLANE_NUM];    /* [o] 每个平面的buffer占用大小, 单位：byte */
    uint32_t    aPlaneElems[RKPQ_MAX_PLANE_NUM];    /* [o] 每个平面的元素数量 */
    uint32_t    nPlaneNum;                          /* [o] 图像的平面数量，由其格式决定 */
    size_t      nFrameSize;                         /* [o] 整幅图像对应的buffer占用大小, 等于所有平面的占用大小之和，单位：byte */

    uint8_t    *aPlaneAddrs[RKPQ_MAX_PLANE_NUM];    /* [i] 每个平面数据的起始虚地址 */
    int32_t     nFdValue;                           /* [i] 输入buffer的硬件描述符，用于实现零拷贝 */
    uint32_t    nFdIndex;                           /* [i] fd的索引，必须是fd的唯一标识符 */
    size_t      nBufferSize;                        /* [i] buffer的总大小, 应 >= nFrameSize，单位：byte */
} rkpq_imgbuf_info;
```

#### 基本概念

SWPQ SDK 支持 YUV 和 RGB 两种系列的图像格式，

**YUV 采样方式**常见的有YUV444，YUV422，YUV420三种，采样比例分别为：1:1:1, 2:1:1, 4:1:1。

**YUV 存储方式**根据其平面数量可分为：
  - **Planar**：Y，U，V分别存储于3个平面
  - **Semi-Plane**：Y存储于1个平面，UV交织存储于1个平面
  - **Interleaved**：Y，U，V交织存储，总计1个平面

**RGB 存储方式**一般为**多通道交织**，所以一般都只有单个平面。

#### 辅助接口的使用 - rkpq_query() with RKPQ_QUERY_IMG_BUF_INFO

1. `rkpq_imgbuf_info`结构体定义了图像的格式、图像尺寸、像素位宽、元素排列方式、所处的色彩空间等基础信息。
2. 在`rkpq_proc_params`结构体中包含了输入/输出图像的这些基础信息，故必须在`rkpq_proc()`接口调用前指定这些信息。
3. 由于`rkpq_imgbuf_info`结构体定义的信息非常详细且复杂，故在`rkpq_query_cmd`中特意增加了`RKPQ_QUERY_IMG_BUF_INFO`条目供`rkpq_query()`接口来自动计算和填充。
4. 工程师可以在设置带有`'[i]'`备注的属性值后，调用`rkpq_query()`接口以获取剩下的属性值。
5. 由于实际图像buffer往往存在对齐量，**自动计算的结果未必和实际情况相符**，所以工程师必须在`rkpq_query()`接口调用后认真检查其属性结果和实际情况是否一致。
6. 当相关属性值与buffer的存储结构不一致时，**应当在`rkpq_query()`调用后，`rkpq_proc()`调用前**，手动修改相关属性值，使其保持一致。
7. 若工程师对`rkpq_imgbuf_info`中所有属性值都了然于胸，在`rkpq_proc()`调用前可直接手动设置各属性，不必调用`rkpq_query()`接口来自动填充。

#### 其他说明

1. 带`[i]`注释部分的内容，**必须**在调用`rkpq_query()`接口前设定，即图像色彩空间、格式、尺寸、元素长度和对齐量等。
2. 带`[i/o]`注释部分的内容，如若值已知**可以**在调用`rkpq_query()`接口前设定，调用后值会被更新。
3. 带`[o]`注释部分的内容，**无须**设定，调用`rkpq_query()`接口后值会被更新。
4. 此结构体指定了输入/输出图像的基本信息，需在调用`rkpq_proc()`之前**确认其值与对应buffer的存储结构保持一致**。
5. 当**调用`rkpq_query()`接口后**，相关属性值与buffer的存储结构不一致时，**应当在`rkpq_query()`调用后，`rkpq_proc()`调用前**，手动修改相关属性值（**包括虚宽 aWidStrides、虚高 aHgtStrides、平面大小 aPlaneSizes、总大小 nFrameSize 等**），使其保持一致。
6. 对齐量`nAlignment`详解：
   - 用于计算图像的虚宽，取值为以2为底的次幂数，如：0, 2, 4, 8, 16, 32, 64, 128等；
   - **默认为第一个平面的对齐量，其他平面的虚宽以第一个平面的虚宽为标准计算，而不是独立计算！**
    ```
    举例：720x480 NV24(YUV444SP)格式，64位对齐，虚宽计算结果对比：
    1) 按SDK中的计算结果：第一个平面的虚宽为align(720,64)=768, 第二个平面的虚宽为768x2=1536;
    2) 按每个平面独立计算，第一个平面虚宽相同为768，第二个平面虚宽为align(720*2,64)=1472，注意这里的不同！
    ```
7. 虚宽属性`aWidStrides[RKPQ_MAX_PLANE_NUM]`详解：
    - 调用`rkpq_query()`后的输出值`aWidStrides[0]`总是大于等于`nPixWid`；
    - 如果已知虚宽为W'，可只设置`aWidStrides[0]=W'`，`aWidStrides[1:2]`将根据图像格式自动计算；
    - 如果已知对齐量`nAlignment`，可以设置`aWidStrides[0]=0`，`aWidStrides[0:2]`将根据图像格式自动计算；
    - 如果buffer的对齐量如第6点所诉每个平面独立计算，或其他与接口返回值不一致的情况，需要对相关属性进行**手动修改**。
    ```
    举例：720x480 NV24(YUV444SP)格式，64位对齐，且虚宽按每个平面独立计算时：
    1) 第一个平面的虚宽为768，将与`rkpq_query()`接口返回值一致，无需修改;
    2) 第二个平面的虚宽为1472，将与`rkpq_query()`接口返回值1536不一致，需手动设置`aWidStrides[1]=1472`.
    3) 虚宽数据被手动修改后，要修改对应平面的buffer大小和图像的帧buffer大小数据，即：
      size_t oldSize = bufInfo.aPlaneSizes[1]; // 记录旧的UV平面buffer大小
      bufInfo.aWidStrides[1] = 1472;  // 修改uv平面虚宽至实际值
      bufInfo.aPlaneSizes[1] = 1472 * bufInfo.aHgtStrides[1]; // 更新uv平面buffer大小
      bufInfo.nFrameSize = bufInfo.nFrameSize - oldSize + bufInfo.aPlaneSizes[1]; // 更新图像帧buffer大小
    ```
8. 虚高属性`aHgtStrides[RKPQ_MAX_PLANE_NUM]`详解：
    - 如果高度方向**没有**对齐填充，设置`aHgtStrides[0]=0`, 调用接口后将会自动更新为`nPixHgt`的值；
    - 如果高度方向**有**对齐填充，设置`aHgtStrides[0]`的值即可，`aHgtStrides[1:2]`将根据图像格式自动计算;
    - 如果只有第1、2个平面之间有对齐填充，第2、3个平面之间没有或者填充量不一致，则也需要在接口返回后对其进行**手动修改**。
9. 虚宽属性`aWidStrides[RKPQ_MAX_PLANE_NUM]`和虚高属性`aHgtStrides[RKPQ_MAX_PLANE_NUM]`经过手动修改后，对应每个平面的占用大小`aPlaneSizes[RKPQ_MAX_PLANE_NUM]`和总占用大小`nFrameSize`也需要对应手动修改！

### rkpq_init_params
```c
/* 初始化参数结构体 */
typedef struct _rkpq_init_params
{
    uint32_t    nInitFlag;          /* 初始化标志位 */
    uint32_t    nExtenFlag;         /* reserved */

    uint32_t    nModNumInPipe;                              /* [i] 自定义 pipeline 中模块的数量 */
    uint32_t    aModPipeOrder[RKPQ_MAX_PIPE_MODULE_NUM];    /* [i] 自定义 pipeline 中模块的顺序，值为rkpq_module枚举值 */
} rkpq_init_params;
```

### rkpq_pipe_fmt_info 和 rkpq_pipe_res_info
```c
/* 格式变换信息，记录在某些会改变输入格式/色彩空间的模块（如CSC/CVT）配置内 */
typedef struct _rkpq_pipe_fmt_info
{
    uint32_t    nSrcClrSpc;     /* [i] 输入图像色彩空间 */
    uint32_t    nSrcPixFmt;     /* [i] 输入图像格式 */
    uint32_t    nSrcDrmFmt;     /* reserved */

    uint32_t    nDstClrSpc;     /* [i] 输出图像色彩空间，如果不是最后一个模块，不可以指定为 Limited-Range ! */
    uint32_t    nDstPixFmt;     /* [i] 输出图像格式 */
    uint32_t    nDstDrmFmt;     /* reserved */
} rkpq_pipe_fmt_info;

/* 分辨率变换信息，记录在某些会改变输入分辨率的模块（如ZME/SR/FE）配置内 */
typedef struct _rkpq_pipe_res_info
{
    uint32_t    nSrcImgWid;   /* [i] 输入图像宽度，单位: pixel */
    uint32_t    nSrcImgHgt;   /* [i] 输入图像高度，单位: pixel */
    uint32_t    nDstImgWid;   /* [i] 输出图像宽度，单位: pixel */
    uint32_t    nDstImgHgt;   /* [i] 输出图像高度，单位: pixel */
} rkpq_pipe_res_info;
```
注：
- 所有输出格式或色彩空间会发生改变的模块（如CSC/CVT）都需要记录当前模块的输入/输出的格式以及色彩空间信息，以便在pipeline之后的模块确认自己的输入格式。
- 所有输出分辨率会发生改变的模块（如ZME/SR/FE等）都需要记录当模块的输入/输出的分辨率信息，以便在pipeline之后的模块确认自己的输入分辨率。


## 应用接口说明
### rkpq_init()
接口作用:
用于初始化一个有效的`rkpq_context`上下文对象，用于后续的处理。

接口定义：
```c
int rkpq_init(rkpq_context *pCtxPtr, rkpq_init_params *pInitParam);
```

参数说明：
- rkpq_context *pCtxPtr: `rkpq_context`对象**指针**
- rkpq_init_params *pInitParam: `rkpq_init_params`结构体对象指针

注：
- 如果没有cl程序缓存，第一次初始化时间会比较长。
- 初始化成功后cl程序会**自动缓存**，之后的初始化耗时将大幅度降低。
- **自动缓存**可能会因为权限等问题失败。
- cl程序默认缓存路径: `/data/rkalgo/` 或 `/data/vendor/rkalgo/`

### rkpq_proc()
接口作用:
根据传入的`rkpq_proc_params`参数，调用`rkpq_context`上下文对象执行PQ处理，处理过程会阻塞调用线程，直到接口程序返回。

接口定义：
```c
int rkpq_proc(rkpq_context ctx, rkpq_proc_params *pProcParam);
```

参数说明：
- rkpq_context ctx: `rkpq_context`对象
- rkpq_proc_params *pProcParam: `rkpq_proc_params`结构体对象指针

### rkpq_deinit()
接口作用:
析构`rkpq_context`上下文对象，回收相关资源。

接口定义：
```c
int rkpq_deinit(rkpq_context ctx);
```

参数说明：
- rkpq_context ctx: 待析构的`rkpq_context`对象

### rkpq_query()
接口作用:
根据`rkpq_query_cmd`条目查询相关信息

接口定义：
```c
int rkpq_query(rkpq_context ctx, rkpq_query_cmd cmd, size_t size, void* info);
```

参数说明：
- rkpq_context ctx: `rkpq_context`对象，对于不需要Context的条目，可直接传入NULL
- rkpq_query_cmd cmd: 查询条目
- size_t size: 返回值的buffer大小
- void* info: 返回值指针

说明：

|       查询条目                     |  是否需要Context  |   返回值类型       |
| --------------------------------- |:-----------------:| ----------------- |
| RKPQ_QUERY_SDK_VERSION            |       no          | rkpq_version_info |
| RKPQ_QUERY_PERF_INFO              |       YES         | rkpq_perf_info    |
| RKPQ_QUERY_IMG_RES_CHANGE_SUPPORT |       no          | uint32_t          |
| RKPQ_QUERY_IMG_FMT_INPUT_SUPPORT  |       no          | rkpq_imgfmt_info  |
| RKPQ_QUERY_IMG_FMT_OUTPUT_SUPPORT |       no          | rkpq_imgfmt_info  |
| RKPQ_QUERY_IMG_FMT_CHANGE_SUPPORT |       no          | uint32_t          |
| RKPQ_QUERY_IMG_COLOR_SPACE_SUPPORT|       no          | rkpq_clrspc_info  |
| RKPQ_QUERY_IMG_BUF_INFO           |       no          | rkpq_imgbuf_info  |
| RKPQ_QUERY_IMG_ALIGNMENT_OCL      |       YES         | uint32_t          |
| RKPQ_QUERY_RKNN_SUPPORT           |       YES         | uint32_t          |
| RKPQ_QUERY_MEAN_LUMA              |       YES         | uint32_t          |
| RKPQ_QUERY_MODULES_SUPPORT        |       YES         | rkpq_module_info  |

### rkpq_set_loglevel()
接口作用:
用于设置SWPQ库的输出log等级，有效值范围[0, 4]，信息量从少到多。

接口定义：
```c
int rkpq_set_loglevel(rkpq_context ctx, int logLevel);
```

参数说明：
- rkpq_context ctx: `rkpq_context`对象，将此参数设为`NULL`使日志等级全局生效。（目前暂不支持按实例设置日志等级）
- int logLevel: 表示log等级，有效值范围[0, 4]

### rkpq_set_cache_path()
接口作用:
用于设置算法库缓存文件路径（存放OpenCL程序缓存，用于程序初始化加速）。
默认路径为`/data/rkalgo/`

接口定义：
```c
int rkpq_set_cache_path(rkpq_context ctx, const char *pPath);
```

参数说明：
- rkpq_context ctx: `rkpq_context`对象，将此参数设为`NULL`使缓存路径全局生效。（目前暂不支持按实例设置缓存路径）
- const char *pPath: 缓存路径字符串，请以'/'结尾

### rkpq_set_default_cfg()
接口作用:
用于还原指定模块的配置参数到默认值。

接口定义：
```c
int rkpq_set_default_cfg(void *pModuleConfig, rkpq_module module);
```

参数说明：
- void *pModuleConfig: SWPQ模块结构体对象指针（指针获取见`rkpq_get_default_cfg()`接口）
- rkpq_module module: SWPQ模块枚举值

### rkpq_get_default_cfg()
接口作用:
用于获取pipeline中指定模块的配置参数指针，以便用户修改相关参数调整该模块的效果。

接口定义：
```c
void *rkpq_get_default_cfg(rkpq_context ctx, int pipeOrder, int module);
```

参数说明：
- rkpq_context ctx: `rkpq_context`对象
- int pipeOrder: 需要设置的模块在pipeline中的顺序索引，需要与`rkpq_init_params`里设定的一致
- int module: `rkpq_module`枚举值

### rkpq_set_inputs() & rkpq_set_outputs()
接口作用:
用于设定pipeline的所有输入图像，主要用于支持多输入或多输出的模块（如MEMC模块）。
支持多帧输入/输出、多种分辨率、多种图像格式，每帧图像的详细完整信息记录在`rkpq_imgbuf_info`结构体内。

接口定义：
```c
int rkpq_set_inputs(rkpq_context ctx, rkpq_imgbuf_info *pInputBufs, int num);
int rkpq_set_outputs(rkpq_context ctx, rkpq_imgbuf_info *pOutputBufs, int num);
```

参数说明：
- rkpq_context ctx: `rkpq_context`对象
- rkpq_imgbuf_info *pInputBufs: 待输入的多帧图像结构体信息数组
- rkpq_imgbuf_info *pOutputBufs: 待输出的多帧图像结构体信息数组
- int num: 输入/输出图像帧数

### rkpq_set_target_platform
接口作用:
用于设定目标运行平台，目前明确支持的平台有：RK3588, RK356X, RK3576.

接口定义：
```c
int rkpq_set_target_platform(const char *pPlatformName);
```

参数说明：
- const char *pPlatformName: 目标平台名称，如"rk3588"、"RK3568"、"rk3576"等，大小写不敏感。


## 调用方法和可执行程序使用介绍
### 集成调用示例
见rkpq_demo.cpp

### 可执行程序使用介绍
使用可执行程序进行仿真前先使用`update_32bit.bat`或`update_64bit.bat`脚本先将可执行程序`rk_swpq_exe`与库文件`librkswpq.so`推送到`/vendor`对应目录下，且添加对应权限。

在板端执行`"adb shell rk_swpq_exe -h"` 获取可执行程序执行说明，这里对控制台参数说一下说明：

执行命令: "adb shell rk_swpq_exe -f=sim" 可对当前swpq算法库编译时指定的流程进行仿真（主要供内部测试使用）；
执行命令: "adb shell rk_swpq_exe -f=unpack" 可对输入的紧凑型YUV文件（如NV15、NV30、RGB565等格式）进行解码，转成非紧凑的图像以便在看图软件上可视化图像；

其中仿真部分的执行命令为: "adb shell rk_swpq_exe -f=sim <`input_options`> <`output_options`> <`proc_flags`>"，其中

`input_options`有:
  - -i=<input_file>, <font color=Red>必填</font>，指定输入文件路径。
  - -sw=<src_width>, <font color=Red>必填</font>，指定输入图像像素宽度。
  - -sh=<src_height>, <font color=Red>必填</font>，指定输入图像像素高度。
  - -sf=<src_format>, <font color=Red>必填</font>，指定输入图像的像素格式，取值为`enum rkpq_img_fmt`。
  - -sc=<src_color_space>, 选填，指定输入图像色彩空间，取值为`enum rkpq_clr_spc`，默认值为1（即`RKPQ_CLR_SPC_YUV_601_FULL`）。
  - -sa=<src_align>, 选填，指定输入图像行方向的对齐量，单位为byte.
  - -sd0=<src_stride0>, 选填，指定输入图像第1个平面虚宽，单位为byte; 未指定情况下会自动计算。
  - -sd1=<src_stride1>, 选填，指定输入图像第2个平面虚宽，单位为byte; 未指定情况下会自动计算。
  - -sd2=<src_stride2>, 选填，指定输入图像第3个平面虚宽，单位为byte; 未指定情况下会自动计算。

`output_options`有:
- -o=<output_file> , 选填，指定输出文件路径，未指定的情况下输出文件将会被设置成`"/data/dump/rkpq_result_WxH.fmt"`。
  可通过log查看输出文件路径。（**请确保输出文件所在路径存在！**）
- -dw=<dst_width>, 选填，指定输出图像像素宽度。若未指定，则与输入一致（会自动64像素对齐）。
- -dh=<dst_height>, 选填，指定输出图像像素高度。若未指定，则与输入一致。
- -df=<dst_format>, 选填，指定输出图像像素格式。若未指定，则与输入一致。
- -dc=<dst_color_space>, 选填，指定输出图像色彩空间。若未指定，则与输入一致。
- -da=<src_align>, 选填，指定输出图像行方向的对齐量，单位为byte.
- -dd0=<dst_stride0>, 选填，指定输出图像第1个平面虚宽，单位为byte；若未指定，则与输入一致。
- -dd1=<dst_stride0>, 选填，指定输出图像第2个平面虚宽，单位为byte；未指定情况下会自动计算。
- -dd2=<dst_stride0>, 选填，指定输出图像第3个平面虚宽，单位为byte；未指定情况下会自动计算。

`proc_flags`有:
  - -csc=0/1, 开关CSC模块。
  - -shp=0/1, 开关SHP模块。
  - -acm=0/1, 开关ACM模块。
  - -dci=0/1, 开关DCI模块。
  - -sr=0/1, 开关SR模块。
  - -zme=0/1, 开关ZME模块。
  <!-- - -pw=<proc_width>, 指定处理宽度，影响SHP、ACM、DCI模块；默认为9600. -->
  - -n=<num_frames>, 指定处理帧数，默认为1帧。

**注：允许adb属性控制时，以上模块开关和某些参数可能会被adb属性强制覆盖！**
程序执行完毕后，依照log提示，将输出文件导出，检查结果。


## debug & tuning 工具
### 通过PQTool进行tuning
见《Rockchip_PQTool_Guide》。

### 通过adb properties进行tuning
在Android系统下可以通过 adb properties 来 debug 和 tuning

支持的 adb properties 有:

| 属性名称 | 取值范围 | 控制行为 | 备注 |
| ------- | -------- | ------- | ----- |
| persist.vendor.rkpq.watermark | {0, 1} | 是否显示水印 | 开启后在零拷贝模式且没有虚地址传入的情况下，会做mmap，影响帧率 |
| persist.vendor.rkpq.loglevel  | [0, 4] | 控制log等级 | log等级超过INFO，会影响帧率 |
| persist.vendor.rkpq.logstep   | [0, 512] | 控制log显示频率 | |
| persist.vendor.rkpq.corenum   | [0, 8] | Reserved |  |
| persist.vendor.rkpq.dump      | [0, 15] | 控制dump行为 | 见后文详解 |
| test.vendor.rkpq.roi.enable   | {0, 1} | 控制ROI行为开关 | 目前仅支持AISR/AIDM模块 |
| persist.vendor.rkpq.all.prop_cover | {0, 1} | **强行覆盖设置标志** | 设1后由adb properties设定的参数会强制覆盖输入参数 |
| persist.vendor.rkpq.all.proc_width | [0, 9600] | Deprecated |  |
| persist.vendor.rkpq.csc.enable | {0, 1} | 开关CSC模块 | Deprecated |
| persist.vendor.rkpq.dci.enable | {0, 1} | 开关DCI模块 | Deprecated |
| persist.vendor.rkpq.acm.enable | {0, 1} | 开关ACM模块 | Deprecated |
| persist.vendor.rkpq.shp.enable | {0, 1} | 开关SHP模块 | Deprecated |
| persist.vendor.rkpq.sr.enable  | {0, 1} | 开关SR模块  | Deprecated |
| persist.vendor.rkpq.zme.enable | {0, 1} | 开关ZME模块 | Deprecated |

**所有可设的属性以文档《pq_tuning.bat》为准。**

### 设置dump行为
```shell
adb shell setprop persist.vendor.rkpq.dump <X>
```
\<X\> 为二进制数`0bxxxx`， **从低到高位**分别表示 dump 所有输入, 所有输出, 单帧输入, 单帧输出。

**有效数值列表**
| X取值 | 二进制值 | dump行为解释        | 生效条件 |
|:-----:|:-------:|:------------------- |:-------- |
| 1     | 0b0001  | dump所有输入        | 在`rkpq_init()`调用前设置此属性才能生效 |
| 2     | 0b0010  | dump所有输出        | 在`rkpq_init()`调用前设置此属性才能生效 |
| 4     | 0b0100  | 按帧dump即时输入    | `/data/dump/rkpq.dump.src.cfg`内指定帧数后生效 |
| 8     | 0b1000  | 按帧dump即时输出    | `/data/dump/rkpq.dump.dst.cfg`内指定帧数后生效 |
| 3     | 0b0011  | dump所有输入+输出   | 同1    |
| 12    | 0b1100  | 按帧dump即时输入+输出| 同4,8 |
| 15    | 0b1111  | 开启全部dump功能    | 对应条件生效后其对应dump功能开启 |

**举例**
```shell
# dump所有输入
adb shell setprop persist.vendor.rkpq.dump 1
# dump所有输出
adb shell setprop persist.vendor.rkpq.dump 2
# 按帧dump输入，从命令生效后开始的连续独立3帧
adb shell setprop persist.vendor.rkpq.dump 4
adb shell "echo 3 >> /data/dump/rkpq.dump.src.cfg"
# 按帧dump输出，从命令生效后开始的连续独立1帧
adb shell setprop persist.vendor.rkpq.dump 8
adb shell "echo 1 >> /data/dump/rkpq.dump.dst.cfg"
```

若要按帧即时dump 5帧输入和输出，则X的值为`0b1100 = 12`
按照一下顺序执行
```shell
# 先执行echo设置帧数
echo 5 >> /data/dump/rkpq.dump.src.cfg          # 命令1
echo 5 >> /data/dump/rkpq.dump.dst.cfg          # 命令2
# 再执行dump行为值设定命令
adb shell setprop persist.vendor.rkpq.dump 12   # 命令3
```
按照以上的顺序执行，可以保证dump下来的输入和输出是相互对应的。
如果先执行命令3，在执行命令1和2，**大概率会造成输入输出不对应的情况**。因为在命令1执行后且命令2执行前，命令1的行为已经生效。

**检查dump是否有效**
在/data/dump目录下检查是否有`rkpq_<W>x<H>_dump_<src/dst>_#<frame_id>_<image_format>.bin`文件生成。
可通过检查log信息来确认dump行为是否有效：
```bash
# dump成功时，log提示示例
: Dump src frame #1566 (size:8294400+4147200+0=12441600), use cmd to pull this file:
:    adb pull <dump_file>

# dump失败时，log提示示例
: Failed to dump src frame #63298, Permission denied
```


## 疑难解答
### 错误码
**OpenCL相关错误码**

OpenCL错误码请见其头文件定义`cl.h`，常见于`rkpq_init()`初始化调用错误中。

**PQ程序错误码**

PQ程序错误码如下：

| 返回值 | 返回值宏名称 | 错误解读 |
| ------ | ---------- | -------- |
| 0  | RK_SUCCESS | 无错误 |
| -1 | RK_ERR_FAILURE | 未知错误 |
| -2 | RK_ERR_TIMEOUT | 超时错误 |
| -3 | RK_ERR_INIT_FAILURE | 初始化错误 |
| -4 | RK_ERR_INVALID_PROCESSOR | 无效的执行对象 |
| -5 | RK_ERR_NULL_POINTER | 空指针错误 |
| -6 | RK_ERR_MALLOC_FAILURE | malloc错误 |
| -7 | RK_ERR_INVALID_VIR_ADDR | 无效的虚地址 |
| -8 | RK_ERR_INVALID_ARGS | 无效的函数实参 |
| -9 | RK_ERR_INVALID_CFGS | 无效的配置参数 |
| -10 | RK_ERR_UNSUPPORTED_DEVICE | 不支持的设备 |
| -11 | RK_ERR_UNSUPPORTED_CASE | 不支持的选项 |
| -12 | RK_ERR_UNSUPPORTED_FEATURE | 不支持的特性 |
| -13 | RK_ERR_UNKNOWN_DEVICE | 未知设备 |
| -14 | RK_ERR_UNKNOWN_CASE | 未知选项 |
| -15 | RK_ERR_UNKNOWN_FEATURE | 未知特性 |
| -16 | RK_ERR_CANCELED | 取消操作 |
| -17 | RK_ERR_DIVISION_BY_ZERO | 除零操作 |
| -20 | RK_ERR_FILE_OPEN_FAIL | 打开文件失败 |
| -21 | RK_ERR_FILE_NOT_EXIST | 文件不存在 |
| -22 | RK_ERR_FILE_NO_PERMISSION | 无权限操作文件 |
| -25 | RK_ERR_DIR_ACCESS_FAIL | 访问目录失败 |
| -26 | RK_ERR_DIR_NOT_EXIST | 目录不存在 |
| -27 | RK_ERR_DIR_NO_PERMISSION | 无权限操作目录 |
| -30 | RK_ERR_LACK_OF_MEMORY | 缺少内存空间 |
| -31 | RK_ERR_OUT_OF_MEMORY | 访问超出内存范围 |
| -32 | RK_ERR_OUT_OF_RANGE | 访问超出有效范围 |
| -33 | RK_ERR_INVALID_MEM_OBJECT | 无效的内存对象 |
| -50 | RK_ERR_NO_INPUT_IMAGE | 未指定输入图像 |
| -51 | RK_ERR_NO_OUTPUT_IMAGE | 未指定输出图像 |
| -52 | RK_ERR_INVALID_IMAGE_FORMAT | 无效的图像格式 |
| -53 | RK_ERR_INVALID_IMAGE_DEPTH | 无效的图像深度 |
| -54 | RK_ERR_INVALID_IMAGE_SIZE | 无效的图像尺寸 |
| -60 | RK_ERR_INVALID_BUFFER_SIZE | 无效的缓冲区大小 |
...

### 常见错误分析解答
1. 输出图像明显被放大且只显示了约原图左上角1/4左右的区域，或仅左上角1/4区域图像有效
    SR或ZME模块没有正确开启/关闭，但输出图像size大小未配置正确。

2. 输出图像亮度/色调/饱和度/对比度怪异
    检查每个开启模块的Config参数是否有初始化，取值是否在合理的范围。可调用`rkpq_set_default_cfg`接口将配置的Config参数设为默认值，再进行相关配置修改。

3. 输出图像全黑，开启水印后有水印显示
    若CSC模块开启，请检查`ConfigCSC`参数是否正确，RGB gain值为0时图像会变黑。

4. 输出图像有重影或者花屏，水印显示不正确
    一般是虚宽配置不对造成。可以打开dump功能，dump输入与输出。根据dump出来的图像分析输入和输出的虚宽是否正确。

5. 输出图像绿屏
  - LOG没有错误：
    可能是该版本SDK未支持的某些模块被开启，可查询`RKPQ_QUERY_MODULES_SUPPORT`条目获取当前版本支持的模块。
  - LOG里有错误"CL_MISALIGNED_SUB_BUFFER_OFFSET":
    可能是输入输出图像buffer没有对齐量，造成OpenCL映射buffer错误，建议修改图像buffer的对齐量。
  - LOG里有错误"No enough src(or dst) buffer size (xxxx) < frame size (xxxx)"
    buffer的大小参数传入错误，应该不小于图像的大小。

6. 输出图像Y通道正常，UV通道有重复性条纹
    - 可能是Y和UV的像素对齐量不一致导致，可以dump输入输出确认Y通道和UV通道的虚宽是否正确。
    - 分辨率较小时往往存在对齐虚宽，图像的对齐量可能与预期不符。
    - UV通道交织于第二个平面时（有2个元素）与Y平面（1个元素）的对齐量计算方法一致，但不符合图像的默认排列方式。

7. 调用`rkpq_init()`错误，初始化失败，可能有如下log输出：
    ```
    --e-- rkpq_init() failed! -46
    Failed to call rk_vop_init()! Exitng...
    ```
    错误码-46为OpenCL相关元素初始化失败报错，请检查以下内容：
    - OpenCL库文件是否存在或有读取权限；
    - OpenCL版本是否大于1.2；
    - 检查OpenCL错误代码（"OpenCL error with code xxxx"）：
      - `CL_INVALID_KERNEL_NAME`，可能是旧版本SDK未规避不支持的模块，请更新使用的SDK版本；也有可能是本地的OpenCL程序文件被破坏或与SDK版本不符，可使用命令`rm /data/rkalgo/*_cl.bin`删除这些文件文件后重新让程序执行初始化；
      - `CL_OUT_OF_HOST_MEMORY`，一般是`rkpq_proc_params::nSrcBufSize`或`rkpq_proc_params::nDstBufSize`设置过大，超出了实际的buffer大小，请检查buffer的实际大小并修正此参数；
      - `CL_INVALID_WORK_GROUP_SIZE`，可能是图像分辨率和大小等信息与实际不符，请检查传入的图像信息参数；
      - `CL_MISALIGNED_SUB_BUFFER_OFFSET`，图像分辨率可能太小，或者对齐量不足，导致OpenCL使用错误，请调大对齐量或者图像分辨率

8. 水印显示有误
    - 检查水印属性是否开启，可通过LOG关键字"watermark"确保水印flag开启，且输出图像宽度大于等于720.
    - 水印和图像均显示错位，大概率是输出图像虚宽配置不正确。
    - 水印显示错位，输出图像显示正常，有可能混淆了图像的宽和高两个参数。