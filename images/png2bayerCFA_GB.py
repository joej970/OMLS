# Reads PNG image, then cuts it to smaller images and saves pixel values in binary.

import sys
import os
import getopt
import png
import numpy as np
import matplotlib.pyplot as plt
import sys
from skimage import measure
import time
import os

def calc_entropy(data, minVal, maxVal):
    bins = range(minVal, maxVal+1)
    freq, bins = np.histogram(data, bins = bins) #, range = (0,2048+1))
    # plt.figure()
    # plt.bar(range(minVal, maxVal), freq, width= 1)
    # plt.show()
    freq = filter(lambda p: p > 0, freq)
    freq = list(freq)
    prob = freq/sum(freq)
    entropy_bayer_manual = -sum(np.multiply(prob, np.log2(prob)))
    return entropy_bayer_manual

def create_folder_if_not_exists(folder_path):
    if not os.path.exists(folder_path):
        os.makedirs(folder_path)
        print(f"Folder '{folder_path}' created.")
    else:
        print(f"Folder '{folder_path}' already exists.")

def print_progress(percentage, note="", newLine=False):
    sys.stdout.write("\r")
    sys.stdout.write(
        "                                                                                            "
    )
    sys.stdout.flush()
    sys.stdout.write("\r")
    if percentage > 100:
        percentage = 100
    sys.stdout.write("[%-20s] %d%%" % ("=" * (percentage // 5), percentage))
    sys.stdout.write(" " + note)
    if newLine:
        sys.stdout.write("\n")
    sys.stdout.flush()
    return percentage


     

def dump_help():
    print("Parameters: ")
    print("   -i (--input_loc): Input location (required, default None). Image can be RGB or grayscale.")
    print("   -o (--output_loc): Output location name (required, default None).")
    print("   -s (--start): Start index for img range (required, default 0).")
    print("   -e (--end): End index for image range (required, default 0).")
    print("   -l (--lossy): [Lossy bits. (default 0 (lossless compression), max 3).]")
    print("   -B (--BayerCFA): [Add -B only if image is already grayscale and already in Bayer Colour Filter Array.]")
    print("   -b (--bpp): [Bits per pixel. (default 8).]")
    print("   -f (--filenameFormat): [Filename format. (default img_).]")


def main(argv):
    input_loc = None
    output_loc = None
    minIdx = None
    maxIdx = None
    lossyBits = 0
    alreadyBayer = False
    filenameFormat = "img_"
    bpp = 8

    print("Hello")

    try:
        opts, args = getopt.getopt(
            argv,
            "hi:o:s:e:l:Bb:f:",
            [
                "input_loc=",
                "output_loc=",
                "start=",
                "end=",
                "lossy=",
                "BayerCFA=",
                "bpp=",
                "filenameFormat="
            ],
        )
    except getopt.GetoptError:
        print("Error: Invalid arguments.")
        dump_help()
        sys.exit(2)


    for opt, arg in opts:
        if opt == "-h":
            dump_help()
            sys.exit()
        elif opt in ("-o", "--output_loc"):
            output_loc = arg
        elif opt in ("-i", "--input_loc"):
            input_loc = arg
        elif opt in ("-s", "--start"):
            minIdx = int(arg)
        elif opt in ("-e", "--end"):
            maxIdx = int(arg)
        elif opt in ("-l", "--lossy"):
            lossyBits = int(arg)
        elif opt in ("-B", "--BayerCFA"):
            alreadyBayer = True
        elif opt in ("-b", "--bpp"):
           bpp = int(arg)
        elif opt in ("-f", "--filenameFormat"):
            filenameFormat = arg

    # if bpp != 8:
    filenameFormatOut = f"{filenameFormat}{bpp}bpp_"
    # else:
        # filenameFormatOut = filenameFormat

    if (input_loc == None) or (output_loc == None):
        print("Error: Missing command line parameters. Try with -h option.")
        sys.exit()

    percentage = 0

    create_folder_if_not_exists(f"{output_loc}")

    print(f"Binary image files will be saved to {output_loc}")

    for imgIdx in range(minIdx, maxIdx + 1):
        percentage = 0
        percentage = print_progress(percentage, note=f"Loading {filenameFormat}{imgIdx:02}.png")

        r = png.Reader(f"{input_loc}/{filenameFormat}{imgIdx:02}.png")
        img = r.read()
        width = img[0]
        height = img[1]
        pngdata = img[2]
        planes = img[3]["planes"]


        percentage = print_progress(percentage + 10, note=f"Reading image data")

        # read image data
        img2d = np.empty((height, width * planes), dtype=np.uint8)
        for i, el in enumerate(pngdata):
            img2d[i, :] = list(map(np.uint8, el))

        img2d = img2d.reshape((height, width, planes))

        crop_image = False
        if width % 16 != 0: 
            print(f"Image width ({width}) is not multiple of 16.")
            width = width - width % 16
            crop_image = True
        if height % 16 != 0:
            print(f"Image height ({height}) is not multiple of 16.")
            height = height - height % 16
            crop_image = True

        if crop_image:
           print("Cropping image to ", width, "x", height, " (muipltiple of 16)")
           img2d = img2d[:height, :width, :]


        # cut border around the image
        # border = 4
        border = 0
        if border != 0:
            img2d = img2d[border:-border, border:-border, :]
            width = width - 2 * border
            height = height - 2 * border

            # Write Cut original to png
            percentage = print_progress(
                percentage + 10, note=f"Write cut original to png"
            )
            w = png.Writer(width=width, height=height, greyscale=False, bitdepth=8)
            f = open(f"{output_loc}/bayerCFA_GB/cut_{filenameFormatOut}{imgIdx:02}.png", "wb")
            imgForPng = img2d.astype(np.uint8).flatten()
            imgForPng = imgForPng.reshape([height, width * 3])
            w.write(f, imgForPng)
            f.close()

        percentage = print_progress(percentage + 10, note=f"Constructing Bayer CFA")
        bayerCFA = np.empty((height, width))
        bayerCFA[:, :] = np.nan
        if planes == 3:
            red_ch = 0
            green_ch = 1
            blue_ch = 2
        else: 
            red_ch = 0
            green_ch = 0
            blue_ch = 0

        if alreadyBayer:
            bayerCFA = img2d[:, :, 0].astype(np.uint8)
        else:
            # Even col even row: GB Green Blue 
            bayerCFA[0::2, 0::2] = img2d[0::2, 0::2, green_ch].astype(
                np.uint8
            )  
            # Odd  col even row: B Blue
            bayerCFA[0::2, 1::2] = img2d[0::2, 1::2, blue_ch].astype(
                np.uint8
            )  
            # Even col odd  row: R Red
            bayerCFA[1::2, 0::2] = img2d[1::2, 0::2, red_ch].astype(
                np.uint8
            )  
            # Odd  col odd  row: Gr Green Red
            bayerCFA[1::2, 1::2] = img2d[1::2, 1::2, green_ch].astype(
                np.uint8
            )  

        # entropy_bayer = measure.shannon_entropy(bayerCFA)
        entropy_bayer = calc_entropy(bayerCFA.flatten(), 0, 256)

        if np.isnan(bayerCFA).any():
            raise Exception("Error! Some NaNs in image arrray!")

        # from float to uint8
        bayerCFA = bayerCFA.astype(np.uint8)

        # Write BayerCFA to png
        percentage = print_progress(percentage + 10, note=f"Write Bayer CFA to png")
        w = png.Writer(width=width, height=height, greyscale=True, bitdepth=8)
        f = open(f"{output_loc}/{filenameFormatOut}{imgIdx:02}.png", "wb")
        w.write(f, bayerCFA)
        f.close()

        ############### Save as bin

        percentage = print_progress(percentage + 10, note=f"Read back exported PNG")

        r = png.Reader(f"{output_loc}/{filenameFormatOut}{imgIdx:02}.png")

        img = r.read()

        width = img[0]
        height = img[1]
        pngdata = img[2]

        # print(pngdata)

        img2d = np.empty((height, width), dtype=np.uint8)
        for i, el in enumerate(pngdata):
            img2d[i, :] = list(map(np.uint8, el))

        temp_img = img2d.ravel()
        if bpp != 8:
            temp_img = temp_img.astype(np.int16)
            temp_img = temp_img << (bpp - 8)
            # add 2-bit random noise
            temp_img = temp_img - np.random.randint(0, 2**(bpp - 8), temp_img.shape).astype(np.uint16)
            # temp_img = temp_img - np.zeros_like(temp_img)
            temp_img[temp_img < 0] = 0
            temp_img = temp_img.astype(np.uint16)


        # entropy_bayer = measure.shannon_entropy(bayerCFA)
        entropy_bayer = calc_entropy(temp_img.flatten(), 0, 2**bpp)

        percentage = print_progress(
            percentage + 10,
            note=f"Export binary {filenameFormatOut}{imgIdx} from shape = {img2d.shape} to shape{temp_img.shape}",
        )

        # delete existing file
        file_path = f"{output_loc}/{filenameFormatOut}{imgIdx:02}.bin"  # Replace with the actual file path

        if os.path.exists(file_path):
            os.remove(file_path)
            # print(f"File {file_path} deleted successfully.")

        with open(f"{output_loc}/{filenameFormatOut}{imgIdx:02}.bin","ab") as output:

            # get current time in ms
            time_ms = int(round(time.time() * 1000))
            timestamp = np.uint64(time_ms)

            unaryLength = np.uint8(8)
            # lossyBits = np.uint8(0)
            reserved = np.uint8(0)

            # header [timestamp 64, width 16, height 16, unaryLength 8, bpp 8, lossyBits 8, reserved 8]
            output.write(np.uint64(timestamp).tobytes())
            output.write(np.uint16(width).tobytes())
            output.write(np.uint16(height).tobytes())
            output.write(np.uint8(unaryLength).tobytes())
            output.write(np.uint8(bpp).tobytes())
            output.write(np.uint8(lossyBits).tobytes())
            output.write(np.uint8(reserved).tobytes())

            # get number of written bytes
            
            output.write(temp_img.tobytes())



        percentage = print_progress(
            100, note=f"{filenameFormatOut}{imgIdx:02} {width} x {height} done! Entropy: {entropy_bayer:0.2f} BPP at {bpp:2d} initial BPP", newLine=True
        )

    print(" ")
    print(f"Done! Images successfully written to {output_loc}/")

if __name__ == "__main__":
    main(sys.argv[1:])