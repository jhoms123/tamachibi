#!/usr/bin/env python3
"""Render the firmware's growth branches and active walking loop."""
from pathlib import Path
import colorsys
import math
import re
import subprocess
import tempfile

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "ESP32_Tamachibi_v47_ESP32U"
GFX = SRC / ".pio/libdeps/esp32u/Adafruit GFX Library"
FAMILIES = [
    "Skyling", "Tidekin", "Pouncer", "Shadelet", "Crumbkin",
    "Hornling", "Pupkin", "Sproutling", "Tinker", "Gearling",
]
BRANCHES = [
    [0, 15, 43, 45, 40], [1, 17, 23, 36, 39], [2, 20, 28, 29, 48],
    [3, 10, 24, 30, 44], [4, 22, 25, 31, 37], [5, 6, 33, 42, 47],
    [7, 16, 26, 35, 41], [8, 9, 19, 27, 46], [11, 12, 13, 18, 49],
    [14, 21, 32, 34, 38],
]
SPECIES = [
    "Bird", "Slug", "ShellCat", "Shadow", "Burger", "HornPig", "LongDino",
    "RoundDog", "Mushroom", "Leaf", "Hood", "Runner", "Square", "Snowman",
    "Hoodie", "MiniBird", "CowBell", "LongSeal", "Tower", "Frog", "BatCat",
    "BatBug", "Bean", "Jelly", "Devil", "Blob", "BigNose", "HatStack",
    "ShirtCat", "Fox", "Candle", "Potato", "Helmet", "BigMouth", "NoseMan",
    "Mouse", "Whale", "Sandwich", "Robot", "Octo", "UFO", "RoundPet",
    "Unicorn", "Duck", "Ghost", "HornOwl", "Cape", "DinoHead", "StarCat", "Bell",
]


def extract_void(filename: str, name: str) -> str:
    source = (SRC / filename).read_text()
    masked = re.sub(
        r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
        lambda match: " " * len(match[0]), source, flags=re.S,
    )
    match = re.search(r"(?m)^void\s+" + re.escape(name) + r"\s*\([^;{}]*\)\s*(?:const\s*)?\{", masked)
    if not match:
        raise ValueError(name)
    pos = match.end()
    depth = 1
    while depth:
        if masked[pos] == "{":
            depth += 1
        elif masked[pos] == "}":
            depth -= 1
        pos += 1
    return source[match.start():pos]


def _font(size: int, bold: bool = False):
    filename = "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"
    return ImageFont.truetype(filename, size)


def centered(draw, xy, text, font, fill, anchor="mm"):
    draw.text(xy, text, font=font, fill=fill, anchor=anchor)


def make_artifact(executable: Path, output: Path):
    with tempfile.TemporaryDirectory(prefix="tamachibi-evolution-") as tmp:
        temp = Path(tmp)
        sprites = temp / "sprites"
        sprites.mkdir()
        program = (ROOT / "tools/preview_fixture.cpp").read_text()
        program += "\n#include \"GrowthSprites.h\"\n"
        program += "\n" + extract_void("GameMain.cpp", "petWalkPose") + "\n"
        program += "\n" + extract_void("GameMain.cpp", "drawSquarePet") + "\n"
        program += r'''
void renderSprite(const char *name, byte family, byte species, byte age, byte route) {
  display.clearDisplay();
  display.useNativeLayout(true);
  NativeUI::pet(display, 32, 32, age < STAGE_ADULT ? family : species,
                age, 0, false, 4, route);
  display.display();
  panel.save(name);
}
int main() {
  char filename[64];
  for (byte family = 0; family < 10; ++family) {
    for (byte age = 0; age <= 3; ++age) {
      const byte routes = age == STAGE_TEEN ? 2 : 1;
      for (byte route = 0; route < routes; ++route) {
        snprintf(filename, sizeof(filename), "sprites/f%u_s%u_r%u.pbm", family, age, route);
        renderSprite(filename, family, 0, age, route);
      }
    }
    for (byte tier = 0; tier < 5; ++tier) {
      const byte species = growthAdultSpecies(family, tier);
      snprintf(filename, sizeof(filename), "sprites/f%u_s4_r%u.pbm", family, tier);
      renderSprite(filename, family, species, STAGE_ADULT, 0);
    }
    const byte elder = growthAdultSpecies(family, 2);
    snprintf(filename, sizeof(filename), "sprites/f%u_s5_r0.pbm", family);
    renderSprite(filename, family, elder, 5, 0);
  }

  // Render stationary adult poses so this preview isolates the articulated
  // body part from the separate home-screen walking animation.
  display.useNativeLayout(true);
  for (byte family = 0; family < 10; ++family) {
    const byte species = growthAdultSpecies(family, 2);
    for (byte pose = 0; pose < 4; ++pose) {
      anim = pose * 2;
      display.clearDisplay();
      NativeUI::pet(display, 48, 48, species, STAGE_ADULT, anim,
                    false, 2);
      display.display();
      snprintf(filename, sizeof(filename), "idle_f%u_p%u.pbm", family, pose);
      panel.save(filename);

      display.clearDisplay();
      for (int y = 48; y < 80; ++y)
        for (int x = 48; x < 80; ++x)
          if (((x + y) & 1) == 0)
            display.drawPixel(x, y, 1);
      NativeUI::pet(display, 48, 48, species, STAGE_ADULT, anim,
                    false, 2);
      display.display();
      snprintf(filename, sizeof(filename), "part_f%u_p%u.pbm", family, pose);
      panel.save(filename);
    }
  }

  stage = STAGE_ADULT;
  speciesType = growthAdultSpecies(2, 3);
  eggType = 2;
  idleStyle = 0;
  sleeping = false;
  lightOff = false;
  dead = false;
  petAway = false;
  for (byte frame = 0; frame < 32; ++frame) {
    anim = frame * 2;
    display.clearDisplay();
    {
      NativeUI::Scope scope(display);
      drawSquarePet();
    }
    display.display();
    snprintf(filename, sizeof(filename), "walk_%02u.pbm", frame);
    panel.save(filename);
  }
  return 0;
}
'''
        driver = temp / "evolution.cpp"
        driver.write_text(program)
        subprocess.run([
            "g++", "-std=c++17", "-DARDUINO=100",
            "-I" + str(ROOT / "tools/host"), "-I" + str(GFX), "-I" + str(SRC),
            str(GFX / "Adafruit_GFX.cpp"), str(SRC / "GameDisplay.cpp"),
            str(SRC / "NativeUI.cpp"), str(SRC / "RosterSprites.cpp"),
            str(SRC / "GrowthSprites.cpp"), str(driver), "-o", str(executable),
        ], check=True)
        subprocess.run([str(executable)], cwd=temp, check=True)

        def load_sprite(family, stage, route=0):
            image = Image.open(sprites / f"f{family}_s{stage}_r{route}.pbm").convert("RGB")
            # Include the animated appendages, which extend beyond the 32px
            # torso mask, while preserving their transparent margins.
            image = image.crop((12, 12, 116, 116))
            # Exterior flood preserves black body interiors in the preview.
            mask = image.convert("L")
            for edge in range(mask.width):
                for point in ((edge, 0), (edge, mask.height-1), (0, edge), (mask.width-1, edge)):
                    if mask.getpixel(point) == 0:
                        ImageDraw.floodfill(mask, point, 128, thresh=0)
            rgba = image.convert("RGBA")
            rgba.putalpha(mask.point(lambda value: 0 if value == 128 else 255))
            return rgba

        scale = 2
        width, height = 1548, 1438
        image = Image.new("RGB", (width * scale, height * scale), "#101722")
        draw = ImageDraw.Draw(image)
        S = lambda value: value * scale
        title = _font(S(32), True)
        subtitle = _font(S(16))
        group_font = _font(S(13), True)
        header_font = _font(S(12), True)
        row_font = _font(S(16), True)
        label_font = _font(S(11))
        note_font = _font(S(11))
        margin, row_label_w, cell_w, top, row_h = 18, 170, 122, 118, 127

        draw.text((S(margin), S(10)), "TEN ORIGINAL TAMACHIBI LINES", font=title, fill="#f3f5ff")
        draw.text((S(margin), S(54)), "Egg → Baby → Child → care-shaped teen → five adult outcomes → elder",
                  font=subtitle, fill="#aab8cf")
        draw.text((S(margin), S(82)),
                  "Both teen routes can reach any adult column; the care record selects the adult form.",
                  font=subtitle, fill="#aab8cf")

        x0 = margin + row_label_w
        groups = [
            (0, 2, "HATCH + GROW"), (3, 4, "TEEN ROUTES"),
            (5, 9, "ADULT FORM BY CARE QUALITY"), (10, 10, "ELDER"),
        ]
        for first, last, label in groups:
            gx = S(x0 + first * cell_w + 4)
            gw = S((last - first + 1) * cell_w - 8)
            draw.rounded_rectangle((gx, S(100), gx + gw, S(126)), radius=S(7), fill="#223149")
            centered(draw, (gx + gw // 2, S(113)), label, group_font, "#d9e7ff")

        columns = ["EGG", "BABY", "CHILD", "THRIVING", "CARE CHALLENGE",
                   "BEST", "GOOD", "MIXED", "ROUGH", "LOW", "ADULT + CANE"]
        for column, label in enumerate(columns):
            left = x0 + column * cell_w
            centered(draw, (S(left + cell_w // 2), S(141)), label, header_font, "#d8deea")

        stage_specs = [(0, 0), (1, 0), (2, 0), (3, 0), (3, 1)]
        for family, family_name in enumerate(FAMILIES):
            y = top + family * row_h
            band = "#172334" if family % 2 == 0 else "#141e2c"
            draw.rounded_rectangle((S(margin), S(y), S(width - margin), S(y + row_h - 5)),
                                   radius=S(9), fill=band)
            hue = family / len(FAMILIES)
            rgb = tuple(round(channel * 255) for channel in colorsys.hsv_to_rgb(hue, 0.55, 0.95))
            draw.rounded_rectangle((S(margin + 8), S(y + 13), S(margin + 13), S(y + row_h - 18)),
                                   radius=S(2), fill=rgb)
            draw.text((S(margin + 24), S(y + 40)), family_name, font=row_font, fill="#f1f4fc", anchor="lm")
            draw.text((S(margin + 24), S(y + 65)), f"LINE {family + 1:02d}", font=note_font, fill="#8493a9", anchor="lm")

            for column, (age, route) in enumerate(stage_specs):
                left = x0 + column * cell_w
                icon = load_sprite(family, age, route).resize((S(72), S(72)), Image.Resampling.NEAREST)
                image.paste(icon, (S(left + (cell_w - 72) // 2), S(y + 5)), icon)

            for tier, species_id in enumerate(BRANCHES[family]):
                column = 5 + tier
                left = x0 + column * cell_w
                icon = load_sprite(family, 4, tier).resize((S(72), S(72)), Image.Resampling.NEAREST)
                image.paste(icon, (S(left + (cell_w - 72) // 2), S(y + 5)), icon)
                centered(draw, (S(left + cell_w // 2), S(y + 83)), SPECIES[species_id], label_font, "#dbe4f3")

            elder = load_sprite(family, 5).resize((S(72), S(72)), Image.Resampling.NEAREST)
            left = x0 + 10 * cell_w
            image.paste(elder, (S(left + (cell_w - 72) // 2), S(y + 5)), elder)
            centered(draw, (S(left + cell_w // 2), S(y + 83)), "chosen adult", label_font, "#dbe4f3")

            # Explicit growth chain, teen fork, and care-selected adult fan.
            centers = [S(x0 + col * cell_w + cell_w // 2) for col in range(11)]
            ink = "#637c9c"
            upper, lower, tip = S(y+101), S(y+117), S(y+94)
            for col in range(2):
                draw.line((centers[col],upper,centers[col+1],upper),fill=ink,width=S(1))
                draw.ellipse((centers[col]-S(2),upper-S(2),centers[col]+S(2),upper+S(2)),fill=ink)
            fork = centers[2]
            draw.line((fork,upper,fork,lower,centers[4],lower),fill=ink,width=S(1))
            for col in (3,4):
                draw.line((centers[col],lower,centers[col],tip),fill=ink,width=S(1))
            join = S(x0+5*cell_w-8)
            draw.line((centers[3],upper,join,upper,join,lower,centers[9],lower),fill=ink,width=S(1))
            draw.line((centers[4],upper,join,upper),fill=ink,width=S(1))
            for col in range(5,10):
                cx=centers[col]
                draw.line((cx,lower,cx,tip),fill=ink,width=S(1))
                draw.polygon([(cx,tip),(cx-S(3),tip+S(4)),(cx+S(3),tip+S(4))],fill=ink)
            centered(draw,(centers[10],S(y+108)),"same adult → elder",note_font,"#8493a9")

        draw.text((S(margin), S(height - 18)),
                  "Adult sprites are original roster designs; elder keeps the chosen adult form and adds a cane.",
                  font=note_font, fill="#8493a9")
        output.parent.mkdir(parents=True, exist_ok=True)
        chart_path = output / "Tamachibi_Evolution_Tree.png"
        image.save(chart_path)

        frames = []
        for frame in range(32):
            raster = Image.open(temp / f"walk_{frame:02d}.pbm").convert("RGB")
            frames.append(raster.resize((512, 512), Image.Resampling.NEAREST))
        gif_path = output / "Tamachibi_Walking_Preview.gif"
        frames[0].save(gif_path, save_all=True, append_images=frames[1:], duration=150,
                       loop=0, optimize=False, disposal=2)
        # Show that the movement loop changes the rendered scene position.
        hashes = [frame.tobytes() for frame in frames]
        if len(set(hashes)) < 12:
            raise RuntimeError("Walking preview did not produce enough distinct frames")
        part_frames = []
        labels = ["SKYLING", "TIDEKIN", "POUNCER", "SHADELET", "CRUMBKIN",
                  "HORNLING", "PUPKIN", "SPROUTLING", "TINKER", "GEARLING"]
        tile_w, tile_h = 110, 127
        for pose in range(4):
            sheet = Image.new("RGB", (tile_w * 5, tile_h * 2), "#101722")
            sheet_draw = ImageDraw.Draw(sheet)
            for family, name in enumerate(labels):
                x0 = (family % 5) * tile_w
                y0 = (family // 5) * tile_h
                sheet_draw.rounded_rectangle((x0 + 3, y0 + 3, x0 + tile_w - 4,
                                              y0 + tile_h - 4),
                                             radius=7, fill="#1b293b")
                sheet_draw.text((x0 + tile_w // 2, y0 + 15), name,
                                font=_font(9, True), fill="#dbe6f7", anchor="mm")
                underlay = Image.open(temp / f"part_f{family}_p{pose}.pbm").convert("RGB")
                sprite = Image.open(temp / f"idle_f{family}_p{pose}.pbm").convert("RGB")
                underlay = underlay.crop((40, 40, 88, 88))
                sprite = sprite.crop((40, 40, 88, 88))
                tile = Image.new("RGB", (48, 48), "#263448")
                tp, bp, sp = tile.load(), underlay.load(), sprite.load()
                for py in range(48):
                    for px in range(48):
                        if sp[px, py][0] > 127:
                            tp[px, py] = (245, 248, 255)
                        elif bp[px, py][0] > 127:
                            tp[px, py] = (77, 96, 120)
                tile = tile.resize((96, 96), Image.Resampling.NEAREST)
                sheet.paste(tile, (x0 + (tile_w - tile.width) // 2, y0 + 23))
            part_frames.append(sheet)
        part_path = output / "Tamachibi_Part_Animation.gif"
        part_frames[0].save(part_path, save_all=True, append_images=part_frames[1:],
                            duration=700, loop=0, optimize=False, disposal=2)
        if len({frame.tobytes() for frame in part_frames}) != 4:
            raise RuntimeError("Lineage part animation did not produce four distinct poses")
        print(chart_path)
        print(gif_path)
        print(part_path)


def main():
    if not (GFX / "Adafruit_GFX.cpp").exists():
        raise SystemExit(f"Adafruit GFX missing: {GFX}")
    output = ROOT.parent
    with tempfile.TemporaryDirectory(prefix="tamachibi-build-") as build:
        make_artifact(Path(build) / "evolution_preview", output)


if __name__ == "__main__":
    main()
