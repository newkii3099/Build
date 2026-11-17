#!/usr/bin/env python3
import os
import sys
from PIL import Image

def generate_icons(logo_path, res_dir):
    if not os.path.exists(logo_path):
        print(f"Error: Logo file '{logo_path}' not found.")
        sys.exit(1)

    img = Image.open(logo_path).convert("RGBA")

    # Make square icon with transparent/padded background if needed or direct resize
    w, h = img.size
    max_dim = max(w, h)
    square_img = Image.new("RGBA", (max_dim, max_dim), (0, 0, 0, 0))
    offset = ((max_dim - w) // 2, (max_dim - h) // 2)
    square_img.paste(img, offset)

    # Densities: (folder, launcher_size, foreground_size)
    densities = [
        ("mipmap-mdpi", 48, 108),
        ("mipmap-hdpi", 72, 162),
        ("mipmap-xhdpi", 96, 216),
        ("mipmap-xxhdpi", 144, 324),
        ("mipmap-xxxhdpi", 192, 432),
    ]

    for folder, l_size, fg_size in densities:
        out_dir = os.path.join(res_dir, folder)
        os.makedirs(out_dir, exist_ok=True)

        # 1. Launcher icon
        l_img = square_img.resize((l_size, l_size), Image.Resampling.LANCZOS)
        l_img.save(os.path.join(out_dir, "ic_launcher.png"), "PNG")
        l_img.save(os.path.join(out_dir, "ic_launcher_round.png"), "PNG")

        # 2. Foreground icon (adaptive icon with ~72% center size inside 108/etc)
        fg_canvas = Image.new("RGBA", (fg_size, fg_size), (0, 0, 0, 0))
        fg_inner_size = int(fg_size * 0.72)
        inner_icon = square_img.resize((fg_inner_size, fg_inner_size), Image.Resampling.LANCZOS)
        fg_offset = ((fg_size - fg_inner_size) // 2, (fg_size - fg_inner_size) // 2)
        fg_canvas.paste(inner_icon, fg_offset)
        fg_canvas.save(os.path.join(out_dir, "ic_launcher_foreground.png"), "PNG")

        # Remove old webp icons if they exist to avoid conflict
        for old_file in ["ic_launcher.webp", "ic_launcher_round.webp"]:
            old_p = os.path.join(out_dir, old_file)
            if os.path.exists(old_p):
                os.remove(old_p)
                print(f"Removed legacy {old_p}")

    print("Icons generated successfully for all mipmap densities!")

if __name__ == "__main__":
    logo = sys.argv[1] if len(sys.argv) > 1 else "app/src/main/res/drawable/logo.png"
    res = sys.argv[2] if len(sys.argv) > 2 else "app/src/main/res"
    generate_icons(logo, res)
