Import("env")

# Access to global construction environment
build_tag = env['PIOENV']

# Read version from platformio.ini [common].pixelit_version
version = env.GetProjectConfig().get("common", "pixelit_version")

prog_name = f"firmware_v{version}_{build_tag}"

print(f"[extra_script] build_tag: {build_tag}")
print(f"[extra_script] version: {version}")
print(f"[extra_script] PROGNAME: {prog_name}")

# Rename binary according to environnement/board
# ex: firmware_esp32dev.bin or firmware_nodemcuv2.bin
env.Replace(PROGNAME=prog_name)
