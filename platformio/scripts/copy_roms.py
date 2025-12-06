Import("env")

import os
import shutil

ROM_EXTENSIONS = (".bin", ".rom")


def copy_rom_assets(source, target, env):
    project_dir = env.subst("$PROJECT_DIR")
    rom_source = os.path.join(project_dir, "roms")
    if not os.path.isdir(rom_source):
        env.LogInfo("No ROM assets found in %s; skipping copy." % rom_source)
        return

    rom_target = os.path.join(env.subst("$BUILD_DIR"), "roms")
    os.makedirs(rom_target, exist_ok=True)

    copied = 0
    for entry in os.listdir(rom_source):
        if entry.lower().endswith(ROM_EXTENSIONS):
            shutil.copy2(os.path.join(rom_source, entry), os.path.join(rom_target, entry))
            copied += 1
    env.LogInfo("Copied %d ROM asset(s) into %s" % (copied, rom_target))


env.AddPreAction("buildprog", copy_rom_assets)
