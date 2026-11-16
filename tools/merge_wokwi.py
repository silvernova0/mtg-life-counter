Import("env")

import os
import subprocess
from os.path import join


def merge_firmware(source, target, env):
    board = env.BoardConfig()
    build_dir = env.subst("$BUILD_DIR")
    merged = join(build_dir, "firmware.merged.bin")
    esptool_name = "esptool.exe" if os.name == "nt" else "esptool"
    esptool = join(env.PioPlatform().get_package_dir("tool-esptool"), esptool_name)
    images = [
        (offset, env.subst(path))
        for offset, path in env.get("FLASH_EXTRA_IMAGES", [])
    ]
    images.append((env.subst("$ESP32_APP_OFFSET"), str(target[0])))

    cmd = [
        esptool,
        "--chip",
        board.get("build.mcu", "esp32"),
        "merge_bin",
        "-o",
        merged,
        "--flash_mode",
        board.get("build.flash_mode", "dio"),
        "--flash_size",
        board.get("upload.flash_size", "4MB"),
    ]
    for offset, path in images:
        cmd.extend([offset if isinstance(offset, str) else hex(offset), path])

    subprocess.check_call(cmd)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_firmware)
