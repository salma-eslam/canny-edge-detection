from PIL import Image
from pathlib import Path
import sys

def raw_to_png(input_raw, output_png, width=512, height=512):
    input_raw = Path(input_raw)
    output_png = Path(output_png)

    if not input_raw.exists():
        print(f"Error: raw file not found: {input_raw}")
        sys.exit(1)

    data = input_raw.read_bytes()

    if len(data) != width * height:
        print(f"Warning: raw size is {len(data)} bytes, expected {width * height}")

    output_png.parent.mkdir(parents=True, exist_ok=True)

    img = Image.frombytes("L", (width, height), data)
    img.save(output_png)

    print(f"Saved {output_png}")

if __name__ == "__main__":
    if len(sys.argv) != 3 and len(sys.argv) != 5:
        print("Usage:")
        print("  python3 tools/raw_to_png.py <input_raw> <output_png>")
        print("  python3 tools/raw_to_png.py <input_raw> <output_png> <width> <height>")
        sys.exit(1)

    input_raw = sys.argv[1]
    output_png = sys.argv[2]

    if len(sys.argv) == 5:
        width = int(sys.argv[3])
        height = int(sys.argv[4])
    else:
        width = 512
        height = 512

    raw_to_png(input_raw, output_png, width, height)

