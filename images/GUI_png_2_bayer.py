
import tkinter as tk
from tkinter import filedialog
import subprocess
from time import sleep

# Global Constants

DEFAULT_IMAGE_FILENAME = "img_"
DEFAULT_BPP = 8


# Function to update custom text
def update_custom_text(text):
    custom_text.delete("1.0", tk.END)
    custom_text.insert(tk.END, text)

def browse_directory(entry):
    folder_selected = filedialog.askdirectory()
    entry.delete(0, tk.END)
    entry.insert(tk.END, folder_selected)

def submit():

    folder_in = folder_in_entry.get()
    folder_out = folder_out_entry.get()
    start_index = int(start_index_entry.get())
    end_index = int(end_index_entry.get())
    lossy_bits = int(lossy_bits_entry.get())
    filename = filename_entry.get()

    if folder_in == "":
        update_custom_text("Please select input folder.\n\n")
        return
    
    if folder_out == "":
        folder_out = folder_in

    print("Filename:", filename)
    print("Folder In:", folder_in)
    print("Folder Out:", folder_out)
    print("Start Index:", start_index)
    print("End Index:", end_index)
    print("Lossy Bits:", lossy_bits)
    print("BPP:", bpp_entry.get())


    # Open a console and run the following command:
        
    call = f"python png2bayerCFA_GB.py -i {folder_in}/original -o {folder_out}/bayerCFA_GB -s {start_index} -e {end_index} -l {lossy_bits} -f {filename} -b {bpp_entry.get()}"
    
    if bayerCFA_grayscale.get():
        call += " -B"


    update_custom_text(f"Calling: {call}\n\n")
    # force update tkinter
    root.update()

    print("Calling subprocess: "+ call)

    exit_code = subprocess.call(call, shell=True)

    if exit_code != 0:
        update_custom_text(f"Error with exit code: {exit_code} (0 for success).\n\n")
    else:
        update_custom_text(f"Sucessfully finished converting .png to .bin.\n\n")

root = tk.Tk()

# Title
root.title("PNG to Bayer CFA GB Binary Converter")
title = tk.Label(root, text="PNG to Bayer CFA GB Binary Converter", font=("Helvetica", 20))
title.pack()
separator = tk.Label(root, height=2)  # Adjust the height for more or less space
separator.pack()

# Filename
filename_label = tk.Label(root, text="Binary (.bin) image filename:")
filename_label.pack()
filename_entry = tk.Entry(root)
filename_entry.insert(0, "img_")
filename_entry.pack()

# Input folder browser
folder_in_label = tk.Label(root, text="Source folder: (folder where folder 'original' with color images is (.png format)")
folder_in_label.pack()

folder_in_entry = tk.Entry(root, width=80)
folder_in_entry.pack()

browse_in_button = tk.Button(root, text="Browse", command=lambda: browse_directory(folder_in_entry))
browse_in_button.pack()

# Compression
bayerCFA_grayscale = tk.BooleanVar()
bayerCFA_grayscale.set(False)
bayerCFA_grayscale_checkbox = tk.Checkbutton(root, text="Is source image grayscale and in bayer pattern?", variable = bayerCFA_grayscale)
bayerCFA_grayscale_checkbox.pack()

# Output folder browser
folder_out_label = tk.Label(root, text="Location where output folder 'bayerCFA_GB' will be created: \n(leave empty to output to source folder)")
folder_out_label.pack()

folder_out_entry = tk.Entry(root, width=80)
folder_out_entry.pack()

browse_out_button = tk.Button(root, text="Browse", command=lambda: browse_directory(folder_out_entry))
browse_out_button.pack()

# Start index for image files
start_index_label = tk.Label(root, text="Start index:")
start_index_label.pack()
start_index_entry = tk.Entry(root, width=10)
start_index_entry.insert(0, "0")
start_index_entry.pack()

# End index for image files
end_index_label = tk.Label(root, text="End index:")
end_index_label.pack()
end_index_entry = tk.Entry(root, width=10)
end_index_entry.insert(0, "9")
end_index_entry.pack()

# Lossy bits (range 0 (default) to 3)
lossy_bits_label = tk.Label(root, text="Lossy bits:")
lossy_bits_label.pack()
lossy_bits_entry = tk.Entry(root, width=10)
lossy_bits_entry.insert(0, "0")
lossy_bits_entry.pack()

# BPP (8,10 or 12)
bpp_label = tk.Label(root, text="BPP:")
bpp_label.pack()
bpp_entry = tk.Entry(root, width=10)
bpp_entry.insert(0, "8")
bpp_entry.pack()




# import os
# print(os.getcwd())

# Submit Button
submit_button = tk.Button(root, text="Submit", command=submit)
submit_button.pack()

# Custom Text Display
custom_text = tk.Text(root, height=5, width=60)
custom_text.pack()

# Example usage


# Set size of window
root.geometry("600x600")

root.mainloop()
