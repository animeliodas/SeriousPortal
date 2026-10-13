"""Oracle for SeriousPortal in the real game (Serious Sam Classic running with SERIOUSBRIDGE_DEV=1, on a level with a
floor and walls in sight, e.g. TFE 11_AlleyOfSphinxes).

    python tools/test_portal.py --game "<Serious Sam folder>"

Commands go to the plugin through %TEMP%\\SeriousPortal\\cmd.txt (the SeriousBridge core dev hook); results are read from the game's log.
  1. a blue portal on the floor a few metres away and an orange one on a wall: both placed, the views through drawn;
  2. dropped from 12 m into the blue one, the player flies out of the wall, as fast as he fell ("speedy thing goes in,
     speedy thing comes out"), sideways;
     (a floor portal shot under one's own feet takes him straight away, as in Portal: so not under the feet)
  3. C (sp_Close) closes them.
Prints PASS/FAIL per check.
"""
import argparse
import math
import re
import tempfile
import time
from pathlib import Path

CMD = Path(tempfile.gettempdir()) / "SeriousPortal" / "cmd.txt"


class Game:
    def __init__(self, folder: str):
        self.log = Path(folder) / "SeriousSam_Custom.log"

    def text(self) -> str:
        return self.log.read_text(encoding="latin-1", errors="replace")

    def run(self, *commands: str, want: str = "", timeout: float = 3.0) -> str:
        """Run console commands; the log written after them, once it has `want`."""
        start = len(self.text())
        CMD.parent.mkdir(exist_ok=True)
        with open(CMD, "a", encoding="ascii") as f:
            for c in commands:
                f.write(c + "\n")
        end = time.time() + timeout
        while time.time() < end:
            time.sleep(0.2)
            new = self.text()[start:]
            if want in new and (want or len(new) > 0):
                time.sleep(0.3)
                return self.text()[start:]
        return self.text()[start:]


def where(game: Game) -> tuple:
    out = game.run("sp_Where();", want="where:")
    m = re.findall(r"where: ([\d.-]+) ([\d.-]+) ([\d.-]+)", out)
    return tuple(float(v) for v in m[-1]) if m else None


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True, help="the Serious Sam Classic folder (its SeriousSam_Custom.log)")
    a = ap.parse_args()
    game = Game(a.game)
    results = []

    game.run("sp_Close();", "cht_bGod=1;")
    start = where(game)
    out = game.run("sp_ShootAt(\"0 1 -0.6 0\");", want="shoot blue")
    blue = re.findall(r"blue portal at \(([\d.-]+), ([\d.-]+), ([\d.-]+)\), facing \(([\d.-]+), ([\d.-]+), ([\d.-]+)\)", out)
    # a wall: try directions round the horizon (a little down, so the far floor isn't hit first, a little up)
    orange = []
    for pitch in (0.0, 0.08, -0.03):
        for k in range(24):
            ang = k * 2.0 * math.pi / 24
            out = game.run(f"sp_ShootAt(\"1 {math.cos(ang):.3f} {pitch:.2f} {math.sin(ang):.3f}\");", want="shoot orange")
            m = re.findall(r"orange portal at \(([\d.-]+), ([\d.-]+), ([\d.-]+)\), facing \(([\d.-]+), ([\d.-]+), ([\d.-]+)\)", out)
            if m and abs(float(m[-1][4])) < 0.3:
                orange = m
                break
        if orange:
            break
    time.sleep(1.5)
    status = game.run("sp_Status();", want="views through")
    views = re.findall(r"(\d+) views through them drawn", status)
    ok = bool(blue) and float(blue[-1][4]) > 0.9 and bool(orange) and views and int(views[-1]) > 0
    results.append(("a floor portal and a wall portal, seen through", ok,
                    f"blue {blue[-1][:3] if blue else 'none'}, orange {orange[-1][:3] if orange else 'none'} facing {orange[-1][3:] if orange else '-'}, "
                    f"views drawn {views[-1] if views else '?'}"))

    if blue and orange:
        b = tuple(float(v) for v in blue[-1][:3])
        p0 = where(game)
        out = game.run(f"sp_Move(\"{b[0] - p0[0]:.2f} {b[1] + 12 - p0[1]:.2f} {b[2] - p0[2]:.2f}\");", want="through the blue portal", timeout=6.0)
        m = re.findall(r"through the blue portal: out at \(([\d.-]+), ([\d.-]+), ([\d.-]+)\), ([\d.]+) m/s \(([\d.-]+), ([\d.-]+), ([\d.-]+)\)", out)
        time.sleep(1.0)
        p = where(game)
        o = tuple(float(v) for v in orange[-1][:3])
        if m:
            speed, vy = float(m[-1][3]), float(m[-1][5])
            near = p is not None and math.dist(p, o) < 40.0
            ok = speed > 8.0 and abs(vy) < 0.5 * speed and near
            results.append(("into the floor portal from 12 m, out of the wall as fast, sideways", ok,
                            f"{speed:.1f} m/s, up {vy:+.1f}; {math.dist(p, o) if p else -1:.1f} m from the orange portal afterwards"))
        else:
            results.append(("into the floor portal from 12 m, out of the wall as fast, sideways", False, "didn't go through"))

    out = game.run("sp_Close();", "sp_Status();", want="portals:")
    results.append(("C closes both", "blue closed, orange closed" in out, re.findall(r"portals: [^;]*", out)[-1] if "portals:" in out else "no status"))

    for name, ok, detail in results:
        print(f"{'PASS' if ok else 'FAIL'}: {name} ({detail})")


if __name__ == "__main__":
    main()
