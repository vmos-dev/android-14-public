/****************************************************************************
 *
 *    Copyright (c) 2023 by Rockchip Corp.  All rights reserved.
 *
 *    The material in this file is confidential and contains trade secrets
 *    of Rockchip Corporation. This is proprietary information owned by
 *    Rockchip Corporation. No part of this work may be disclosed,
 *    reproduced, copied, transmitted, or used in any way for any purpose,
 *    without the express written permission of Rockchip Corporation.
 *
 *****************************************************************************/
#pragma once

#include <memory>

#include "SrType.h"
#include "SrImage.h"

class Sr;

class SvepSr
{
public:
    /**
     * @brief 获取 SvepSr 上下文
     */
    SvepSr();

    /**
     * @brief SvepSr 上下文析构函数
     */
    ~SvepSr();
    /**
     * @brief 禁用引用构造函数
     */
    SvepSr(const SvepSr &) = delete;
    /**
     * @brief 禁用拷贝构造函数
     */
    SvepSr &operator=(const SvepSr &) = delete;

    /**
     * @brief 外部传入的版本信息，用于版本适配，
     *        通常为宏定义SR_VERSION，定义位于 SrType.h 头文件；
     *
     * @param version_str [IN] 版本号，用于版本校验
     * @param async_init  [IN] 异步初始化标志, 设置为 true 表示使能
     *                         异步初始化，主要目的为了避免阻塞调用线程；
     *
     * @return SrError::
     *         - None, success
     *         - Other, fail
     */
    SrError Init(const char *version_str, bool async_init);

    /**
     * @brief 设置SR增强强度，RK3588支持，RK356x不支持，
     *
     * @param rate [IN] 强度值，0-10
     *                  0：最低强度处理，性能最优，建议设置为0
     *                  10：最高强度处理，负载较高，效果最明显
     * @return SrError::
     *         - None, success
     *         - Other, fail
     */
    SrError SetEnhancementRate(int rate);

    /**
     * @brief 设置 OSD 字幕模式
     *
     * @param mode [IN] 模式类型
     * @param osdStr [IN] OSD显示字符串设置，可通过传递字符串的形式更改OSD内容
     * @return SrError::
     *         - None, success
     *         - Other, fail
     */
    SrError SetOsdMode(SrOsdMode mode, const wchar_t *osdStr);

    /**
     * @brief 设置对比模式，提供SR增强与源数据的对比模式展示
     *
     * @param enable [IN] 模式使能开关，未设置则关闭左右对比模式
     * @param offsetPercent [IN] 分割线左右占比，offsetPercent = (SR图像 /
     *                           完整图像)，可实现扫描线效果，设置范围[0]与[10,90]，
     *                           设置为0，即为动态扫描模式，内部会自动偏移扫描线
     * @return SrError::
     *         - None, success
     *         - Other, fail
     */
    SrError SetContrastMode(bool enable, int offsetPercent);

    /**
     * @brief 设置旋转模式
     *
     * @param rotate [IN] 设置旋转方向
     * @return SrError::
     *         - None, success
     *         - Other, fail
     */
    SrError SetRotateMode(SrRotateMode rotate);

    /**
     * @brief 匹配SR算法模型，SR根据不同的输入分辨率，独立实现了不同的算法模型
     *
     * @param int_src  [IN] 输入图像信息
     * @param usage    [IN] 输出SR处理模式特殊标志，目前仅输入4K->8K模式
     * @param out_mode [OUT] 输出SR匹配模式
     * @return SrError::
     *         - None, success
     *         - Other, fail
     */
    SrError MatchSrMode(const SrImageInfo *int_src, SrModeUsage usage,
                        SrMode *out_mode);
    /**
     * @brief 获取SR输出图像参数要求
     *
     * @param out_dst  [IN] out_dst 主要返回 SrBufferInfo 与 SrRect 两个图像信息
     *                      SrBufferInfo ：包含 width/height/stride 等信息
     *                      SrRect：包含真实图像矩形区域坐标，left/top/right/bottom
     * @return SrError::
     *         - None, success
     *         - Other, fail
     */
    SrError GetDetImageInfo(SrImageInfo *out_dst);

    /**
     * @brief 同步处理模式，SR算法完全执行完成后返回
     *
     * @param int_src [IN] 输入图像信息
     * @param int_dst [IN] 输出图像信息，要符合 GetDetImageInfo 接口返回要求
     * @return SrError:
     *         - None, success
     *         - Other, fail
     */
    SrError Run(const SrImageInfo *int_src, const SrImageInfo *int_dst);

    /**
     * @brief 异步处理模式，利用Pipeline处理内部算法流程，图像连续处理可提高帧率
     *
     * @param int_src [IN] 输入图像信息
     * @param int_dst [IN] 输出图像信息，要符合 GetDetImageInfo 接口返回要求
     * @param outFence [OUT]  是栅栏文件描述符，一种可跨进程传递的同步信号文件
     *                        描述符，Signal信号发出后可标志SR任务完成
     * @return SrError:
     *         - None, success
     *         - Other, fail
     */
    SrError RunAsync(const SrImageInfo *int_src, const SrImageInfo *int_dst,
                     int *outFence);

private:
    std::shared_ptr<Sr> mSr_;
};