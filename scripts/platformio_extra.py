import os

Import("env")

framework_libs = env.PioPlatform().get_package_dir(
    "framework-arduinoespressif32-libs"
)
framework = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
fatfs_include = os.path.join(
    framework_libs, env.BoardConfig().get("build.mcu"), "include", "fatfs"
)
fatfs_includes = [
    os.path.join(fatfs_include, "vfs"),
    os.path.join(fatfs_include, "src"),
    os.path.join(fatfs_include, "diskio"),
]
for include_dir in fatfs_includes:
    if not os.path.isdir(include_dir):
        raise FileNotFoundError(
            f"ESP-IDF FatFS include directory not found: {include_dir}"
        )

env.Append(CPPPATH=fatfs_includes)

sd_source = os.path.join(framework, "libraries", "SD", "src")
if not os.path.isdir(sd_source):
    raise FileNotFoundError(f"Arduino-ESP32 SD library not found: {sd_source}")

framework_includes = [
    sd_source,
    os.path.join(framework, "libraries", "FS", "src"),
    os.path.join(framework, "libraries", "SPI", "src"),
]
for include_dir in framework_includes:
    if not os.path.isdir(include_dir):
        raise FileNotFoundError(
            f"Arduino-ESP32 library include directory not found: {include_dir}"
        )

env.Append(CPPPATH=framework_includes)
env.BuildSources("$BUILD_DIR/FrameworkSD", sd_source)
