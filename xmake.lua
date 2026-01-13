includes("VC-LTL5.lua")

add_rules("mode.debug", "mode.release")

set_warnings("more")

add_defines("WIN32", "_WIN32")
add_defines("UNICODE", "_UNICODE", "_CRT_SECURE_NO_WARNINGS", "_CRT_NONSTDC_NO_DEPRECATE")

if is_mode("release") then
    add_defines("NDEBUG")
    add_cxflags("/O2", "/Os", "/Gy", "/MT", "/EHsc", "/fp:precise")
    add_ldflags("/DYNAMICBASE", "/LTCG")
end

add_cxflags("/utf-8")

add_links("gdiplus", "kernel32", "user32", "gdi32", "winspool", "comdlg32")
add_links("advapi32", "shell32", "ole32", "oleaut32", "uuid", "odbc32", "odbccp32")

target("detours")
    set_kind("static")
    add_includedirs("detours/src", {public=true})
    add_files(
        "detours/src/detours.cpp",
        "detours/src/disasm.cpp",
        "detours/src/image.cpp",
        "detours/src/modules.cpp"
    )
    if is_arch("x86") then
        add_defines("_X86_")
        add_files("detours/src/disolx86.cpp")
    elseif is_arch("x64") then
        add_defines("_AMD64_")
        add_files("detours/src/disolx64.cpp")
    elseif is_arch("arm64") then
        add_defines("_ARM64_")
        add_files("detours/src/disolarm64.cpp")
    end

target("vivaldi_plus")
    set_kind("shared")
    set_languages("c++20")
    set_targetdir("$(builddir)/$(mode)/$(arch)")
    set_basename("version")
    add_deps("detours")
    add_files("src/*.cpp")
    add_files("src/*.rc")
    add_links("user32")
    after_build(function (target)
        local builddir = "$(builddir)/$(mode)/$(arch)"
        os.rm(builddir .. "/version.exp")
        os.rm(builddir .. "/version.lib")
    end)
