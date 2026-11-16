Import("env")

def merge_bin_action(source, target, env):
    # Retrieve paths
    flash_size = env.BoardConfig().get("upload.flash_size", "16MB")
    bootloader_bin = "$BUILD_DIR/bootloader.bin"
    partitions_bin = "$BUILD_DIR/partitions.bin"
    firmware_bin = "$BUILD_DIR/firmware.bin"
    merged_bin = "$BUILD_DIR/firmware.merged.bin"

    esptool_cmd = [
        '"$PYTHONEXE"',
        "-m", "esptool",
        "--chip", "esp32s3",
        "merge-bin",
        "-o", merged_bin,
        "--flash-mode", "dio",
        "--flash-freq", "80m",
        "--flash-size", flash_size,
        "0x0000", bootloader_bin,
        "0x8000", partitions_bin,
        "0x10000", firmware_bin
    ]
    
    # Try using platform's tool-esptool directly if python -m esptool is not in global env
    import os
    pio_home = os.path.expanduser("~/.platformio")
    esptool_exe = os.path.join(pio_home, "packages", "tool-esptool", "esptool.exe")
    if os.path.isfile(esptool_exe):
        esptool_cmd[0] = f'"{esptool_exe}"'
        esptool_cmd = [esptool_cmd[0]] + esptool_cmd[3:]

    cmd = " ".join(esptool_cmd)
    env.Execute(cmd)

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin_action)
