package libpq

import (
    "android/soong/android"
    "android/soong/cc"
    "fmt"
    "strings"
)

var SUPPORT_TARGET_PLATFORM = [...]string{
    "rk3588",
    "rk3576",
}

func init() {
    fmt.Println("libpq want to conditional Compile")
    android.RegisterModuleType("cc_libpq_prebuilt_library", libpqFactory)
}

func libpqFactory() (android.Module) {
    module := cc.PrebuiltSharedLibraryFactory()
    android.AddLoadHook(module, libpqPrebuiltLibrary)
    return module
}

func libpqPrebuiltLibrary(ctx android.LoadHookContext) {

    type props struct {
        Multilib struct {
            Lib64 struct {
                Srcs []string
            }
            Lib32 struct {
                Srcs []string
            }
        }
    }
    p := &props{}

    p.Multilib.Lib64.Srcs = getlibpqLibrary(ctx, "arm64")
    p.Multilib.Lib32.Srcs = getlibpqLibrary(ctx, "arm")
    ctx.AppendProperties(p)
}

func checkEnabled(ctx android.LoadHookContext) bool {
    var soc string = getTargetSoc(ctx)
    for i := 0; i < len(SUPPORT_TARGET_PLATFORM); i++ {
        if (strings.EqualFold(SUPPORT_TARGET_PLATFORM[i], soc)) {
            fmt.Println("libpq enabled on " + soc)
            return true
        }
    }
    fmt.Println("libpq disabled on " + soc)
    return false
}

func getlibpqLibrary(ctx android.LoadHookContext, arch string) ([]string) {
    var src []string
    var soc string = getTargetSoc(ctx)
    var prefix string = soc

    if (!checkEnabled(ctx)) {
        prefix = "RK3588"
    }
    src = append(src, "lib/" + prefix + "/" + arch + "/libpq.so")
    return src
}

func getTargetSoc(ctx android.LoadHookContext) (string) {
    var target_board_platform string = strings.ToUpper(ctx.AConfig().Getenv("TARGET_BOARD_PLATFORM"))
    return target_board_platform
}
