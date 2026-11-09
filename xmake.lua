-- 4Strands builds the way 4Stim does: xmake with libxse/commonlibf4 as a submodule,
-- pinned to the same commit as 4Stim's lib/commonlibf4 so the two plugins match.
set_xmakever("3.0.0")

includes("lib/commonlibf4")

set_project("4Strands")
set_version("0.1.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.releasedbg")
set_defaultmode("releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- OStrands' fluid solver: no game code, so it builds and can be tested on its own.
target("FourStrandsFluid")
    set_kind("static")
    add_files("src/fluid/*.cpp")
    add_headerfiles("src/fluid/*.h")
    add_includedirs("src", { public = true })

-- offline checks of the fluid kinds, canals and sources: xmake build FluidTests && xmake run FluidTests
target("FluidTests")
    set_kind("binary")
    set_default(false)
    add_files("tests/fluid_tests.cpp")
    add_deps("FourStrandsFluid")

target("4Strands")
    add_rules("commonlibf4.plugin", {
        name = "4Strands",
        author = "4Strands team",
        description = "Procedural strands, climax fluids and residue for 4Stim scenes"
    })

    add_deps("FourStrandsFluid")
    add_files("src/*.cpp")
    add_headerfiles("src/*.h", "extern/4stim/*.h", "extern/softbodyfo4/*.h")
    add_includedirs("src", "extern/4stim", "extern/softbodyfo4")
    set_pcxxheader("src/PCH.h")
