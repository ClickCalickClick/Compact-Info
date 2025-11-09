#!/usr/bin/env python3
from PIL import Image
import os

# Icon files to resize
icons = [
    'battery.png',
    'battery-full.png',
    'battery-low.png',
    'battery-medium.png',
    'battery-warning.png',
    'sun.png',
    'cloud.png',
    'cloud-rain.png',
    'cloud-snow.png',
    'cloud-lightning.png'
]

resources_dir = 'resources'

for icon in icons:
    input_path = os.path.join(resources_dir, icon)
    if os.path.exists(input_path):
        # Open the original image
        img = Image.open(input_path)
        print(f"Original {icon}: {img.size}")
        
        # Resize to 14x14 using high-quality Lanczos resampling
        img_resized = img.resize((14, 14), Image.Resampling.LANCZOS)
        
        # Save back to the same file
        img_resized.save(input_path)
        print(f"Resized {icon} to 14x14")
    else:
        print(f"Warning: {icon} not found")

print("\nAll icons resized to 14x14!")
