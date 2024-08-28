import tkinter as tk
from tkinter import filedialog
import subprocess
import os

class CompressDecompressGUI:

    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Compress/Decompress GUI")
        self.filename_entry = None
        self.folder_in_entry = None
        self.folder_out_entry = None
        self.start_index_entry = None
        self.end_index_entry = None
        self.lossy_bits_entry = None
        self.bpp_entry = None
        self.compression_en = None
        self.decompression_en = None
        self.command_text = tk.Text(self.root, height=5, width=60)
        self.output_text = None
        self.construct_gui()
        self.attach_callbacks()

    def update_command_text(self, text):
        self.command_text.delete("1.0", tk.END)
        self.command_text.insert(tk.END, text)
        self.root.update()

    def construct_command(self):
        if self.folder_in_entry != None:
            folder_in = self.folder_in_entry.get()
            folder_out = self.folder_out_entry.get()
            start_index = int(self.start_index_entry.get())
            end_index = int(self.end_index_entry.get())
            lossy_bits = int(self.lossy_bits_entry.get())
            filename = self.filename_entry.get()

            if folder_in == "":
                self.update_output_text("Please select input and output folders.\n\n")
                return

            if folder_out == "":
                folder_out = folder_in

            call = f"main.exe -i {folder_in} -o {folder_out} -s {start_index} -e {end_index} -l {lossy_bits} -b 16 -f {filename} -r {self.bpp_entry.get()}"

            if self.decompression_en.get():
                call += " -d"
            if self.compression_en.get():
                call += " -c"


            return call

        else:
            return ""
        
    def attach_callbacks(self):
        self.filename_var.trace_add('write', lambda *args: self.update_command_text(self.construct_command()))     
        self.folder_in_var.trace_add('write', lambda *args: self.update_command_text(self.construct_command()))
        self.folder_in_var.trace_add('write', lambda *args: self.update_command_text(self.construct_command()))
        self.folder_out_var.trace_add('write', lambda *args: self.update_command_text(self.construct_command()))
        self.start_index_var.trace_add('write', lambda *args: self.update_command_text(self.construct_command()))
        self.end_index_var.trace_add('write', lambda *args: self.update_command_text(self.construct_command()))
        self.lossy_bits_var.trace_add('write', lambda *args: self.update_command_text(self.construct_command()))
        self.bpp_var.trace_add('write', lambda *args: self.update_command_text(self.construct_command()))
        self.compression_en.trace_add('write', lambda *args: self.update_command_text(self.construct_command()))
        self.decompression_en.trace_add('write', lambda *args: self.update_command_text(self.construct_command()))



    def construct_gui(self):
        # Title
        title = tk.Label(self.root, text="Compress/Decompress GUI", font=("Helvetica", 20))
        title.pack()
        # Display current working directory
        cwd_label = tk.Label(self.root, text=f"main.exe will be searched for in: {os.getcwd()}")
        cwd_label.pack()
        separator = tk.Label(self.root, height=2)
        separator.pack()

        # Filename
        self.filename_var = tk.StringVar()
        filename_label = tk.Label(self.root, text="Binary (.bin) image filename:")
        filename_label.pack()
        self.filename_entry = tk.Entry(self.root, textvariable=self.filename_var)
        self.filename_entry.insert(0, "img_")
        self.filename_entry.pack()

        # Input folder browser
        self.folder_in_var = tk.StringVar()
        folder_in_label = tk.Label(self.root, text="Folder: (location where 'bayerCFA_GB' folder with .bin files is located)")
        folder_in_label.pack()
        self.folder_in_entry = tk.Entry(self.root, width=80, textvariable=self.folder_in_var)
        self.folder_in_entry.pack()
        browse_in_button = tk.Button(self.root, text="Browse", command=self.browse_directory_in)
        browse_in_button.pack()

        # Output folder browser
        self.folder_out_var = tk.StringVar()
        folder_out_label = tk.Label(self.root, text="Output folder (where 'compressed' and 'decompressed' folders will be created.):\n(leave empty to output to input folder)")
        folder_out_label.pack()
        self.folder_out_entry = tk.Entry(self.root, width=80, textvariable=self.folder_out_var)
        self.folder_out_entry.pack()
        browse_out_button = tk.Button(self.root, text="Browse", command=self.browse_directory_out)
        browse_out_button.pack()

        # Start index for image files
        self.start_index_var = tk.StringVar()
        start_index_label = tk.Label(self.root, text="Start index:")
        start_index_label.pack()
        self.start_index_entry = tk.Entry(self.root, width=10, textvariable=self.start_index_var)
        self.start_index_entry.insert(0, "0")
        self.start_index_entry.pack()

        # End index for image files
        self.end_index_var = tk.StringVar()
        end_index_label = tk.Label(self.root, text="End index:")
        end_index_label.pack()
        self.end_index_entry = tk.Entry(self.root, width=10, textvariable=self.end_index_var)
        self.end_index_entry.insert(0, "9")
        self.end_index_entry.pack()

        # Lossy bits (range 0 (default) to 3)
        self.lossy_bits_var = tk.StringVar()
        lossy_bits_label = tk.Label(self.root, text="Lossy bits:")
        lossy_bits_label.pack()
        self.lossy_bits_entry = tk.Entry(self.root, width=10, textvariable=self.lossy_bits_var)
        self.lossy_bits_entry.insert(0, "0")
        self.lossy_bits_entry.pack()

        # BPP (8,10 or 12)
        self.bpp_var = tk.StringVar()
        bpp_label = tk.Label(self.root, text="BPP:")
        bpp_label.pack()
        self.bpp_entry = tk.Entry(self.root, width=10, textvariable=self.bpp_var)
        self.bpp_entry.insert(0, "8")
        self.bpp_entry.pack()

        # Compression
        self.compression_en = tk.BooleanVar()
        self.compression_en.set(True)
        compression_en_checkbox = tk.Checkbutton(self.root, text="Compression", variable=self.compression_en)
        compression_en_checkbox.pack()

        # Decompression
        self.decompression_en = tk.BooleanVar()
        self.decompression_en.set(False)
        decompression_en_checkbox = tk.Checkbutton(self.root, text="Decompression", variable=self.decompression_en, onvalue=True, offvalue=False)
        decompression_en_checkbox.pack()

        # Submit Button
        submit_button = tk.Button(self.root, text="Submit", command=self.submit)
        submit_button.pack()

        # Command Text Display
        command_label = tk.Label(self.root, text="Command:")
        command_label.pack()
        self.command_text = tk.Text(self.root, height=5, width=60)
        self.command_text.pack()

        # Output Text Display
        output_label = tk.Label(self.root, text="Output:")
        output_label.pack()
        self.output_text = tk.Text(self.root, height=5, width=60)
        self.output_text.pack()

        # Set size of window
        self.root.geometry("600x800")

    def browse_directory_in(self):
        folder_selected = filedialog.askdirectory()
        self.folder_in_entry.delete(0, tk.END)
        self.folder_in_entry.insert(tk.END, folder_selected)

    def browse_directory_out(self):
        folder_selected = filedialog.askdirectory()
        self.folder_out_entry.delete(0, tk.END)
        self.folder_out_entry.insert(tk.END, folder_selected)

    def update_output_text(self, text):
        self.output_text.delete("1.0", tk.END)
        self.output_text.insert(tk.END, text)
        self.root.update()

    def submit(self):
        call = self.construct_command()
        self.update_output_text(f"Calling: {call}\n\n")
        exit_code = subprocess.call(call, shell=True)

        if exit_code != 0:
            self.update_output_text(f"Error with exit code: {exit_code} (0 for success).\n\n")
        else:
            self.update_output_text(f"Successfully finished compression/decompression.\n\n")

    def run(self):
        self.root.mainloop()

gui = CompressDecompressGUI()
gui.run()