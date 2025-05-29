// Copyright (C) 2023 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package  hw_output

import (
        "android/soong/android"
        "android/soong/cc"
        "fmt"
)

func init() {
    // resister a module "hw_output_defaults"
    fmt.Println("hw_output_defaults want to conditional Compile")
    android.RegisterModuleType("cc_hw_output", HwOutputDefaultsFactory)
}

func HwOutputDefaultsFactory() (android.Module) {
    fmt.Println("hw_output_defaults HwOutputDefaultsFactory")
    module := cc.DefaultsFactory()
    android.AddLoadHook(module, HwOutputDefaults)
    return module
}

func HwOutputDefaults(ctx android.LoadHookContext) {
    fmt.Println("hw_output_defaults HwOutputDefaults")
    type props struct {
        Srcs []string
        Cflags []string
        Shared_libs []string
        Include_dirs []string
    }
    p := &props{}
    p.Cflags = getCflags(ctx)
    p.Shared_libs = getSharedLibs(ctx)
    p.Srcs = getSrcs(ctx)
    p.Include_dirs = getIncludeDirs(ctx)
    ctx.AppendProperties(p)
}

func getCflags(ctx android.BaseContext) ([]string) {
    fmt.Println("hw_output_defaults getCflags")
    var cppflags []string
    ret := ctx.AConfig().Getenv("BOARD_USES_HWC_PROXY_SERVICE")
    fmt.Println("BOARD_USES_HWC_PROXY_SERVICE ret:",ret)
    if (ctx.AConfig().IsEnvTrue("BOARD_USES_HWC_PROXY_SERVICE")) {
        cppflags = append(cppflags,"-DUSE_HWC_PROXY_SERVICE")
        fmt.Println("hw_output_defaults add DUSE_HWC_PROXY_SERVICE")
    }
    return cppflags
}

func getSharedLibs(ctx android.BaseContext) ([]string) {
    fmt.Println("hw_output_defaults getSharedLibs")
    var libs []string

    if (ctx.AConfig().IsEnvTrue("BOARD_USES_HWC_PROXY_SERVICE")) {
        libs = append(libs, "libbase")
        libs = append(libs, "libbinder_ndk")
        libs = append(libs, "rockchip.hwc.proxy.aidl-V1-ndk")
        libs = append(libs, "librkhwcproxy")
    }
    return libs
}

func getSrcs(ctx android.BaseContext) ([]string) {
    var src []string

    if (ctx.AConfig().IsEnvTrue("BOARD_USES_HWC_PROXY_SERVICE")) {
        src = append(src, "rk_hwc_proxy_client.cpp")
    }
    return src
}

func getIncludeDirs(ctx android.BaseContext) ([]string) {
    var dirs []string

    if (ctx.AConfig().IsEnvTrue("BOARD_USES_HWC_PROXY_SERVICE")) {
        dirs = append(dirs, "hardware/rockchip/hwc_proxy_service/rockchip/hwc/proxy/drm_api")
    }
    return dirs
}
