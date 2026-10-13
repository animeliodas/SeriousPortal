"""Install SeriousPortal into Serious Sam Classic, with Portal's sounds taken from the player's own Portal.

    uv run -q --with vpk python tools/install_portal.py --tfe "<TFE folder>" --tse "<TSE folder>" [--portal "<Portal folder>"]
    uv run -q --with vpk python tools/install_portal.py --tfe ... --uninstall

- the plugin: plugin/dist/<config>/SeriousPortal.dll -> <game>/Bin/Plugins/
  (Serious Sam Classics Patch 1.9.4 must be installed there: Bin/ClassicsCore.dll);
- the sounds: read from Portal's portal_pak_dir.vpk (found through Steam's libraries, or --portal), converted with
  ffmpeg to 16-bit PCM WAV (what Serious Engine plays) -> <game>/Sounds/SeriousPortal/. They are Valve's: they stay on
  this machine and are never part of this repository. Without Portal (or ffmpeg) the portals just make no sound.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = {"tfe": ROOT / "plugin" / "dist" / "Release_TFE105" / "SeriousPortal.dll",
          "tse": ROOT / "plugin" / "dist" / "Release_TSE107" / "SeriousPortal.dll"}
# what the plugin plays -> Portal's file
SOUNDS = {
    "shoot_blue": "sound/weapons/portalgun/portalgun_shoot_blue1.wav",
    "shoot_orange": "sound/weapons/portalgun/portalgun_shoot_red1.wav",
    "open": "sound/weapons/portalgun/portal_open2.wav",
    "close": "sound/weapons/portalgun/portal_close1.wav",
    "invalid": "sound/weapons/portalgun/portal_invalid_surface3.wav",
    "enter": "sound/player/portal_enter1.wav",
}


def steam_libraries() -> list[Path]:
    libs = []
    for steam in (Path(r"C:\Program Files (x86)\Steam"), Path(r"C:\Program Files\Steam")):
        vdf = steam / "steamapps" / "libraryfolders.vdf"
        if vdf.exists():
            libs += [Path(p.replace("\\\\", "\\")) for p in re.findall(r'"path"\s+"([^"]+)"', vdf.read_text(encoding="utf-8", errors="replace"))]
    return libs


def find_portal(given: str | None) -> Path | None:
    candidates = [Path(given)] if given else [lib / "steamapps" / "common" / "Portal" for lib in steam_libraries()]
    for c in candidates:
        if (c / "portal" / "portal_pak_dir.vpk").exists():
            return c
    return None


def find_ffmpeg() -> str | None:
    found = shutil.which("ffmpeg")
    if found:
        return found
    winget = Path(os.environ.get("LOCALAPPDATA", "")) / "Microsoft" / "WinGet" / "Packages"
    for exe in winget.glob("*FFmpeg*/**/bin/ffmpeg.exe"):
        return str(exe)
    return None


def install_sounds(game: Path, portal: Path, ffmpeg: str) -> int:
    import vpk  # (uv run --with vpk)

    pak = vpk.open(str(portal / "portal" / "portal_pak_dir.vpk"))
    out = game / "Sounds" / "SeriousPortal"
    out.mkdir(parents=True, exist_ok=True)
    done = 0
    with tempfile.TemporaryDirectory() as tmp:
        for name, path in SOUNDS.items():
            try:
                data = pak.get_file(path).read()
            except KeyError:
                print(f"  {path}: not in this Portal")
                continue
            raw = Path(tmp) / f"{name}.src.wav"
            raw.write_bytes(data)
            # Serious Engine plays plain PCM with the data right after the format (no LIST chunk); Portal's are partly ADPCM
            r = subprocess.run([ffmpeg, "-v", "error", "-y", "-i", str(raw), "-map_metadata", "-1", "-fflags", "+bitexact",
                                "-flags:a", "+bitexact", "-acodec", "pcm_s16le", "-ar", "44100", "-ac", "1",
                                str(out / f"{name}.wav")], capture_output=True, text=True)
            if r.returncode == 0:
                done += 1
            else:
                print(f"  {path}: ffmpeg failed: {r.stderr.strip()[:200]}")
    return done


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--tfe")
    ap.add_argument("--tse")
    ap.add_argument("--portal", help="Portal's folder (default: found through Steam's libraries)")
    ap.add_argument("--uninstall", action="store_true")
    ap.add_argument("--sounds-only", action="store_true", help="only the sounds (the game may be running)")
    a = ap.parse_args()
    games = {k: Path(v) for k, v in (("tfe", a.tfe), ("tse", a.tse)) if v}
    if not games:
        sys.exit("give --tfe and/or --tse (the games' folders)")

    portal = None if a.uninstall else find_portal(a.portal)
    ffmpeg = None if a.uninstall else find_ffmpeg()
    if not a.uninstall:
        print(f"Portal: {portal or 'not found (no sounds)'}; ffmpeg: {ffmpeg or 'not found (no sounds)'}")

    for game, folder in games.items():
        plugins = folder / "Bin" / "Plugins"
        if not (folder / "Bin" / "ClassicsCore.dll").exists():
            print(f"{game}: Classics Patch isn't installed in {folder} (no Bin/ClassicsCore.dll); skipped")
            continue
        target = plugins / "SeriousPortal.dll"
        sounds = folder / "Sounds" / "SeriousPortal"
        if a.uninstall:
            target.unlink(missing_ok=True)
            shutil.rmtree(sounds, ignore_errors=True)
            print(f"{game}: SeriousPortal removed")
            continue
        if not PLUGIN[game].exists():
            print(f"{game}: build the plugin first (plugin/build.sh)")
            continue
        plugins.mkdir(parents=True, exist_ok=True)
        if not a.sounds_only:
            shutil.copy2(PLUGIN[game], target)
        n = install_sounds(folder, portal, ffmpeg) if portal and ffmpeg else 0
        print(f"{game}: installed {target}; {n}/{len(SOUNDS)} Portal sounds in {sounds}")


if __name__ == "__main__":
    main()
