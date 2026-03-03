import argparse
from datetime import datetime
from pathlib import Path
import subprocess
import time
import serial


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Read ADC capture from Pico and plot it.")
    parser.add_argument("--port", required=True, help="Serial port (e.g. /dev/tty.usbmodem1101)")
    parser.add_argument("--baud", type=int, default=230400, help="Serial baud rate")
    parser.add_argument("--kp", type=float, required=True, help="Kp value to send")
    parser.add_argument("--ki", type=float, required=True, help="Ki value to send")
    parser.add_argument("--timeout", type=float, default=2.0, help="Serial timeout in seconds")
    parser.add_argument("--save-dir", default="plots", help="Directory for PNG outputs")
    parser.add_argument("--no-show", action="store_true", help="Skip interactive plot window")
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    indices = []
    actual_voltages = []
    desired_voltages = []

    ser = serial.Serial(args.port, args.baud, timeout=args.timeout)
    try:
        print(f"Opened port: {ser.name}")
        time.sleep(0.2)
        ser.reset_input_buffer()

        gains_line = f"{args.kp} {args.ki}\n".encode("utf-8")
        ser.write(gains_line)
        ser.flush()

        while True:
            raw = ser.readline()
            if not raw:
                continue

            text = raw.decode("utf-8", errors="ignore").strip()
            parts = text.split()
            if len(parts) != 3:
                continue

            try:
                idx = int(parts[0])
                actual_val = int(parts[1])
                desired_val = int(parts[2])
            except ValueError:
                continue

            indices.append(idx)
            actual_voltages.append(actual_val)
            desired_voltages.append(desired_val)
            print(f"{idx} {actual_val} {desired_val}")

            if idx == 0:
                break

    finally:
        ser.close()
        print("Closed port.")

    real_check_output = subprocess.check_output

    def check_output_patched(*p_args, **p_kwargs):
        cmd = p_args[0] if p_args else p_kwargs.get("args", [])
        if (
            isinstance(cmd, (list, tuple))
            and len(cmd) >= 3
            and cmd[0] == "system_profiler"
            and "SPFontsDataType" in cmd
        ):
            return b"""<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
  <dict>
    <key>_items</key>
    <array/>
  </dict>
</array>
</plist>
"""
        return real_check_output(*p_args, **p_kwargs)

    subprocess.check_output = check_output_patched

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    plt.figure()
    plt.plot(indices, actual_voltages, "r*-", label="Actual")
    plt.plot(indices, desired_voltages, "b.-", label="Desired")
    plt.ylabel("Voltage (ADC counts)")
    plt.xlabel("Index")
    plt.title(f"Phototransistor Capture (kp={args.kp}, ki={args.ki})")
    plt.grid(True)
    plt.legend()

    out_dir = Path(args.save_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    out_file = out_dir / f"capture_kp_{args.kp:g}_ki_{args.ki:g}_{timestamp}.png"
    plt.savefig(out_file, dpi=150, bbox_inches="tight")
    print(f"Saved plot: {out_file.resolve()}")

    if not args.no_show:
        try:
            matplotlib.use("MacOSX")
        except Exception:
            pass
        plt.show()


if __name__ == "__main__":
    main()
