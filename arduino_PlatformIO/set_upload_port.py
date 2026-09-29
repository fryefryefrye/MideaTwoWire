Import("env")
import os, re

project_dir = env.subst("$PROJECT_DIR")

# Upload port: COM port or OTA IP, modify upload_ip_com_port.txt without recompiling
with open(os.path.join(project_dir, "upload_ip_com_port.txt"), encoding="utf-8") as f:
    port = ""
    for raw_line in f:
        line = raw_line.strip()
        if not line or line.startswith("#") or line.startswith(";") or line.startswith("//"):
            continue
        # Support inline comments, e.g. COM5 # USB
        line = line.split("#", 1)[0].split(";", 1)[0].strip()
        if line:
            port = line
            break

if not port:
    raise ValueError("No active upload target found in upload_ip_com_port.txt")

env.Replace(UPLOAD_PORT=port)
# Auto-select upload protocol based on port type
if re.match(r"^\d+\.\d+\.\d+\.\d+$", port) or "." in port:
    env.Replace(UPLOAD_PROTOCOL="espota")
else:
    env.Replace(UPLOAD_PROTOCOL="esptool")