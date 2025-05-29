package hdmicec
import (
        "android/soong/android"
        "android/soong/cc"
//        "fmt"
        "strings"
)
func init() {
    //fmt.Println("hdmicec module init start")
    android.RegisterModuleType("hdmicec", DefaultsFactory)
}
func DefaultsFactory() (android.Module) {
    module := cc.DefaultsFactory()
    android.AddLoadHook(module, Defaults)
    return module
}
func Defaults(ctx android.LoadHookContext) {
    type props struct {
        Cflags []string
    }
    p := &props{}
    p.Cflags = globalDefaults(ctx)
    ctx.AppendProperties(p)
}
func globalDefaults(ctx android.BaseContext) ([]string) {
    var cppflags []string
    // fmt.Println("HDMI_PORT_TYPE:",
    //     ctx.AConfig().Getenv("HDMI_PORT_TYPE"))
    if strings.EqualFold(ctx.AConfig().Getenv("HDMI_PORT_TYPE"),"in") {
        cppflags = append(cppflags,"-DHDMI_PORT_TYPE=in")
    }else{
        cppflags = append(cppflags,"-DHDMI_PORT_TYPE=out")
    }
    return cppflags
}
