"""Radar display for the ultrasonic radar

Reads lines from the Arduino in either format:
    angle,distance_cm
    angle,distance_cm,checksum    (bad checksums are rejected)
Lines starting with '#' are status messages and get printed.
"""
import argparse
import csv
import math
import time

import matplotlib.pyplot as plt
import serial

MAX_RANGE_CM = 250    # edge of the display
ALERT_CM = 50         # readings closer than this are drawn red (match the Arduino)
FADE_SECONDS = 5.0    # dots fade out over this long
MIN_ANGLE, MAX_ANGLE = 15, 165


def parse_line(line):
    """Turn one line from the Arduino into (angle, distance_cm).

    Returns None for status lines, garbage, or a failed checksum.
    Raises ValueError("checksum") when the checksum doesn't match, so the
    caller can count bad lines separately from garbage.
    """
    line = line.strip()
    if not line or line.startswith("#"):
        return None

    parts = line.split(",")
    if len(parts) == 3:
        body = parts[0] + "," + parts[1]
        expected = sum(body.encode()) % 256   # same rule as the Arduino
        if int(parts[2]) != expected:
            raise ValueError("checksum")
    elif len(parts) != 2:
        return None

    angle = int(parts[0])
    distance = float(parts[1])
    if not (0 <= angle <= 180) or distance < 0:
        return None
    return angle, distance


def to_xy(angle_deg, distance_cm):
    """Polar (angle, distance) to x/y. 90 degrees is straight ahead (+y)."""
    a = math.radians(angle_deg)
    return distance_cm * math.cos(a), distance_cm * math.sin(a)


class RadarDisplay:
    def __init__(self):
        self.readings = {}   # angle -> (distance_cm, time received)
        self.current_angle = 90
        self.good = 0
        self.bad_checksum = 0
        self.garbage = 0
        self.status = ""

        plt.ion()
        self.fig, self.ax = plt.subplots(figsize=(10, 6), facecolor="black")
        self.fig.canvas.manager.set_window_title("Ultrasonic Radar")
        self.ax.set_facecolor("black")
        self.ax.set_aspect("equal")
        self.ax.set_xlim(-MAX_RANGE_CM - 10, MAX_RANGE_CM + 10)
        self.ax.set_ylim(-15, MAX_RANGE_CM + 10)
        self.ax.axis("off")
        self._draw_grid()

        self.dots = self.ax.scatter([], [], s=25)
        (self.sweep_line,) = self.ax.plot([], [], color="lime", lw=2, alpha=0.8)
        self.info = self.fig.text(0.02, 0.02, "", color="lime", family="monospace", fontsize=10)

    def _draw_grid(self):
        green = "#1f6f3f"
        arc = [math.radians(d) for d in range(0, 181, 2)]
        for r in range(50, MAX_RANGE_CM + 1, 50):           # range rings every 50 cm
            self.ax.plot([r * math.cos(t) for t in arc], [r * math.sin(t) for t in arc],
                         color=green, lw=0.8)
            self.ax.text(r, -10, f"{r} cm", color=green, fontsize=8, ha="center")
        for d in range(0, 181, 30):                          # angle spokes every 30 deg
            x, y = to_xy(d, MAX_RANGE_CM)
            self.ax.plot([0, x], [0, y], color=green, lw=0.6)
            lx, ly = to_xy(d, MAX_RANGE_CM + 8)
            self.ax.text(lx, ly, f"{d}°", color=green, fontsize=8, ha="center", va="center")

    def handle_line(self, line):
        """Process one line. Returns (angle, distance) if it was a good reading."""
        if line.strip().startswith("#"):
            self.status = line.strip()[1:].strip()
            print("Arduino:", self.status)
            return None
        try:
            result = parse_line(line)
        except ValueError as err:
            if str(err) == "checksum":
                self.bad_checksum += 1
            else:
                self.garbage += 1
            return None
        if result is None:
            if line.strip():
                self.garbage += 1
            return None
        angle, distance = result
        self.readings[angle] = (distance, time.time())
        self.current_angle = angle
        self.good += 1
        return result

    def redraw(self):
        now = time.time()
        points, colors = [], []
        for angle, (dist, t) in self.readings.items():
            age = now - t
            if dist <= 0 or dist > MAX_RANGE_CM or age > FADE_SECONDS:
                continue
            points.append(to_xy(angle, dist))
            alpha = max(0.15, 1 - age / FADE_SECONDS)
            colors.append((1, 0.2, 0.2, alpha) if dist < ALERT_CM else (0.2, 1, 0.4, alpha))

        self.dots.set_offsets(points if points else [[float("nan"), float("nan")]])
        self.dots.set_facecolors(colors if colors else [(0, 0, 0, 0)])

        x, y = to_xy(self.current_angle, MAX_RANGE_CM)
        self.sweep_line.set_data([0, x], [0, y])

        last = self.readings.get(self.current_angle, (0, 0))[0]
        last_text = f"{last:.1f} cm" if last > 0 else "no echo"
        self.info.set_text(
            f"angle {self.current_angle:3d}°  {last_text:>10}   |   "
            f"good {self.good}  bad checksum {self.bad_checksum}  garbage {self.garbage}"
            + (f"   |   {self.status}" if self.status else "")
        )
        self.fig.canvas.draw_idle()


def main():
    parser = argparse.ArgumentParser(description="Ultrasonic radar display")
    parser.add_argument("port", help="serial port, e.g. COM5 or /dev/cu.usbmodem1101")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--log", help="save every good reading to this CSV file")
    args = parser.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0)  # timeout=0: never wait
    time.sleep(2)               # opening the port resets the UNO; give it time to boot
    ser.reset_input_buffer()    # drop anything half-sent during the reset

    log_file = open(args.log, "w", newline="") if args.log else None
    log = csv.writer(log_file) if log_file else None
    if log:
        log.writerow(["time_s", "angle_deg", "distance_cm"])

    display = RadarDisplay()
    buffer = b""
    start = time.time()
    try:
        while plt.fignum_exists(display.fig.number):   # stop when the window is closed
            buffer += ser.read(ser.in_waiting or 1)
            while b"\n" in buffer:
                raw, buffer = buffer.split(b"\n", 1)
                line = raw.decode("ascii", errors="replace")
                reading = display.handle_line(line)
                if reading and log:
                    log.writerow([round(time.time() - start, 3), *reading])
            display.redraw()
            plt.pause(0.03)
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
        if log_file:
            log_file.close()
        print(f"Done: {display.good} good, {display.bad_checksum} bad checksum, "
              f"{display.garbage} garbage lines")


if __name__ == "__main__":
    main()
