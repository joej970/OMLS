import os
from PIL import Image

# Directory containing the .ppm files
input_directory = 'C:/DATA/Repos/Article_OMLS/images/rgb8bit/rgb8bit/'
output_directory = 'C:/DATA/Repos/Article_OMLS/images/rgb8bit/converted/'

# Create output directory if it doesn't exist
if not os.path.exists(output_directory):
    os.makedirs(output_directory)

# Iterate over all files in the input directory
for filename in os.listdir(input_directory):
    if filename.endswith('.ppm'):
        # Open the .ppm file
        with Image.open(os.path.join(input_directory, filename)) as img:
            # Convert the image to .png format
            png_filename = os.path.splitext(filename)[0] + '.png'
            img.save(os.path.join(output_directory, png_filename), optimize=False, compress_level=0)

print("Conversion complete.")