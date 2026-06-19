from PIL import Image, ImageOps
from pathlib import Path
import sys

def convert_image(input_path, output_raw, width=512, height=512):
    input_path = Path(input_path)
    output_raw = Path(output_raw)

    if not input_path.exists():
        print(f"Error: input file not found: {input_path}")
        sys.exit(1)

    output_raw.parent.mkdir(parents=True, exist_ok=True)

    img = Image.open(input_path).convert("L")
    img = ImageOps.fit(img, (width, height), method=Image.Resampling.LANCZOS)

    output_raw.write_bytes(img.tobytes())

    print(f"Created {output_raw}")
    print(f"Size: {output_raw.stat().st_size} bytes")
    print(f"Expected: {width * height} bytes")

if __name__ == "__main__":
    if len(sys.argv) != 3 and len(sys.argv) != 5:
        print("Usage:")
        print("  python3 tools/convert_image.py <input_image> <output_raw>")
        print("  python3 tools/convert_image.py <input_image> <output_raw> <width> <height>")
        print("")
        print("Example:")
        print("  python3 tools/convert_image.py input/conan.jpeg input/conan_512.raw")
        sys.exit(1)

    input_image = sys.argv[1]
    output_raw = sys.argv[2]

    if len(sys.argv) == 5:
        width = int(sys.argv[3])
        height = int(sys.argv[4])
    else:
        width = 512
        height = 512

    convert_image(input_image, output_raw, width, height)
