#!/usr/bin/env python3
"""Print the HomeKit pairing payload (and a scannable QR code) for a setup code.

    python3 tools/homekit_qr.py 31415926 [SETUP_ID]

The payload layout matches HomeSpan's HapQR: X-HM:// + 9 base-36 digits +
the 4-character Setup ID. The QR code is drawn with `qrencode` when it's
installed (brew install qrencode); otherwise only the payload is printed.
"""
import shutil
import subprocess
import sys

CATEGORY_AIR_CONDITIONER = 21
FLAG_IP = 2
DIGITS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"


def payload(code: int, setup_id: str, category: int = CATEGORY_AIR_CONDITIONER) -> str:
    n = (category << 31) | (FLAG_IP << 27) | (code & 0x07FFFFFF)
    out = ""
    for _ in range(9):
        out = DIGITS[n % 36] + out
        n //= 36
    return f"X-HM://{out}{setup_id[:4]}"


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__.strip())
        return 1
    digits = sys.argv[1].replace("-", "")
    setup_id = sys.argv[2] if len(sys.argv) > 2 else "MRCL"
    if len(digits) != 8 or not digits.isdigit():
        print("Setup code must be 8 digits, e.g. 31415926 or 314-15-926")
        return 1
    if len(setup_id) != 4 or not setup_id.isalnum():
        print("Setup ID must be 4 letters/digits, e.g. MRCL")
        return 1

    text = payload(int(digits), setup_id)
    print(f"Setup code : {digits[:3]}-{digits[3:5]}-{digits[5:]}")
    print(f"Payload    : {text}\n")
    if shutil.which("qrencode"):
        subprocess.run(["qrencode", "-t", "ANSIUTF8", "-m", "2", text], check=False)
        print("Scan this with the iPhone Camera or the Home app.")
    else:
        print("Install qrencode to draw the code here (brew install qrencode), or paste")
        print("the payload into any QR code generator.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
