# SSD1327 Image Converter

Convert PNG/JPG artwork into 128x128 Arduino `PROGMEM` arrays for the Sesame SSD1327 OLED.

Install Pillow once:

```powershell
python -m pip install pillow
```

Generate a high-quality dithered monochrome bitmap compatible with the current firmware:

```powershell
python .\image-converter\convert_ssd1327_image.py .\art\happy.png --symbol epd_bitmap_happy --output .\happy_bitmap.h --mode mono
```

Generate native SSD1327 4-bit grayscale data:

```powershell
python .\image-converter\convert_ssd1327_image.py .\art\happy.png --symbol epd_gray_happy --output .\happy_gray.h --mode gray4
```

Use source images at least 512x512 when possible. Simple faces with strong shapes, clean contrast, and limited tiny detail read best on a 128x128 OLED.
