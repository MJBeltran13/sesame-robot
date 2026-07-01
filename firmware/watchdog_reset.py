Import("env")

import os
import subprocess


def watchdog_reset_after_upload(source, target, env):
    port = env.subst("$UPLOAD_PORT") or env.GetProjectOption("upload_port")
    if not port:
        return

    platform = env.PioPlatform()
    esptool_dir = platform.get_package_dir("tool-esptoolpy")
    esptool = os.path.join(esptool_dir, "esptool.py")
    python = env.subst("$PYTHONEXE")

    subprocess.run(
        [
            python,
            esptool,
            "--chip",
            "esp32s3",
            "-p",
            port,
            "--before",
            "no_reset",
            "--after",
            "watchdog_reset",
            "run",
        ],
        check=False,
    )


env.AddPostAction("upload", watchdog_reset_after_upload)
