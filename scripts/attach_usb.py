Import("env")
import subprocess

# Requires a one-time `usbipd bind --busid 2-1` (admin) on the host.
# No-op outside WSL / when usbipd is unavailable, so manual flashing still works.
USBIPD_BUSID = "2-1"

def attach(*args, **kwargs):
    try:
        subprocess.call(["usbipd.exe", "attach", "--wsl", "--busid", USBIPD_BUSID])
    except FileNotFoundError:
        print("usbipd.exe not found - skipping WSL USB attach")

env.AddPreAction("upload", attach)
