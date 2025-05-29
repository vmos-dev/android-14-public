package rvcamvintf

import (
    "android/soong/android"
    "android/soong/vintf-compatibility-matrix"
    "fmt"
    "strconv"
    "unsafe"
)

func rvcamvintfDefaults(ctx android.LoadHookContext) {
    type props struct {
        Srcs []string
    }

    platformVersion := ctx.AConfig().PlatformVersionName()
    platformVersionInt, err := strconv.Atoi(*(*string)(unsafe.Pointer(&platformVersion)))
    if err != nil {
        fmt.Printf("cannot get platformVersion, %q could not be parsed as an integer\n", platformVersion)
        panic(1)
    }

    p := &props{}
    var srcs []string
    if platformVersionInt == 14 {
        srcs = append(srcs, "rvcam_compatibility_matrix_u.xml")
    } else if platformVersionInt == 12 {
        srcs = append(srcs, "rvcam_compatibility_matrix_s.xml")
    }
    p.Srcs = srcs
    ctx.AppendProperties(p)
}

func init() {
    android.RegisterModuleType("rvcam_vintf_compatibility_matrix", rvcamvintfFactory)
}


func rvcamvintfFactory() android.Module {
    module := vintf.VintfCompatibilityMatrixFactory()
    android.AddLoadHook(module, rvcamvintfDefaults)
    return module
}
