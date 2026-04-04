from pathlib import Path

from SCons.Script import Import

Import("env")


def disable_lvgl_helium():
    project_dir = Path(env.subst("$PROJECT_DIR"))
    helium_src = (
        project_dir
        / ".pio"
        / "libdeps"
        / env.subst("$PIOENV")
        / "lvgl"
        / "src"
        / "draw"
        / "sw"
        / "blend"
        / "helium"
        / "lv_blend_helium.S"
    )
    disabled_src = helium_src.with_suffix(".S.disabled")

    if helium_src.exists():
        helium_src.rename(disabled_src)
        print("Disabled incompatible LVGL Helium assembly for ESP32-S3:", helium_src)


disable_lvgl_helium()
