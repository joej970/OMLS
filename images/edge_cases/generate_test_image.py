import png
import numpy as np
import matplotlib.pyplot as plt
import sys
import os
import argparse


def main(argv):


    parser = argparse.ArgumentParser(description="Image Compression and Decompression")


    parser.add_argument("-s", "--type_start", type=int, default=0, help="Type of image to generate, start idx. See file for details.")
    parser.add_argument("-e", "--type_end", type=int, default=0, help="Type of image to generate, end idx. See file for details.")
    
    args = parser.parse_args()

    # if no argumets are given, generate all images
    if len(sys.argv) == 1:
        args.type_start = 0
        args.type_end = 7

    # Original byte size: 2048*2048 = 4096 kB
    # Original byte size: 512*512= 262 kB

    # idx 7: checkerboard size 1
    # idx 6: checkerboard size 4
    # idx 5: checkerboard size 8
    # idx 4: checkerboard size 16
    # idx 3: BAYER [0 255 255 255; 255 0 0 0] => [765 -255 -255 0; 255 255 255 0] YCCC (probably the worst possible pattern that can be is still compressed: 4053kB) just slight compression. Added white strip in beggining to ensure better compression.
    # idx 2: BAYER [0 255 255 255; 255 0 0 0] => [765 -255 -255 0; 255 255 255 0] YCCC (probably the worst possible pattern that can be is still compressed: 4053kB) compressed size is larger. Added white strip in beggining to change compression ratio.
    # idx 1: BAYER [0 0 0 255; 255 0 255 0 ] => [765 -255 -255 0; 510 255 255 -255] YCCC with repetitive 104 leading zeros to bring A to 0 (maybe actually the worst possible pattern: ULTIMATE WORST: 6699 kB)
    # idx 0: BAYER [0 255 255 255; 255 0 0 0] => [765 -255 -255 0; 255 255 255 0] YCCC with repetitive 104 leading zeros to bring A to 0 (actually the worst possible pattern: 5257 kB)


    # cam = 1

    # r = png.Reader(f"images/driveNscan/bayerCFA_GB/PTCAM{cam:02}_{imgIdx}.png")

    # img = r.read()
    width = 512
    height = 512

    if args.type_start > args.type_end:
        print("Start index is larger than end index. Exiting.")
        return
    
    if args.type_start < 0 or args.type_end > 7:
        print("Start index or end index is out of bounds [0,7]. Exiting.")
        return

    for imgIdx in range(args.type_start, args.type_end+1):

        print(f"Generating image {imgIdx}...")

        img2d = 0*np.ones((height, width), dtype=np.uint8)

        if imgIdx == 0:
            matrix1 = np.array([[255, 255],
                                [255, 0]])

            matrix2 = np.array([[0, 0],
                                [0, 255]])

            pattern0 = np.hstack((matrix1, matrix2))
            pattern1 = np.hstack((matrix2, matrix1))
            for row in range(0,height,4):
                for col_pattern_start_idx in range(13*4*2,width-13*4*2,2*13*4*2):
                    for col in range(col_pattern_start_idx, col_pattern_start_idx + 13*4*2, 4): # max A is 7140, it needs to be halved 13 times to become 0. Thus 13*4 same channels. Each channel repeats itself on every second pixel. So 13*4*2 pixels.
                        img2d[row:row+2,col:col+4] = pattern0
                        img2d[row+2:row+4,col:col+4] = pattern1
        elif imgIdx == 1:
            matrix1 = np.array([[255, 255],
                                [255, 0]])

            matrix2 = np.array([[0, 255],
                                [0, 255]])

            pattern0 = np.hstack((matrix1, matrix2))
            pattern1 = np.hstack((matrix2, matrix1))
            for row in range(0,height,4):
                for col_pattern_start_idx in range(13*4*2,width-13*4*2,2*13*4*2):
                    for col in range(col_pattern_start_idx, col_pattern_start_idx + 13*4*2, 4): # max A is 7140, it needs to be halved 13 times to become 0. Thus 13*4 same channels. Each channel repeats itself on every second pixel. So 13*4*2 pixels.
                        img2d[row:row+2,col:col+4] = pattern0
                        img2d[row+2:row+4,col:col+4] = pattern1

        elif imgIdx == 2:
            matrix1 = np.array([[255, 255],
                                [255, 0]])

            matrix2 = np.array([[0, 0],
                                [0, 255]])

            pattern0 = np.hstack((matrix1, matrix2))
            pattern1 = np.hstack((matrix2, matrix1))
            for row in range(0,height,4):
                for col in range(0,width,4):
                    img2d[row:row+2,col:col+4] = pattern0
                    img2d[row+2:row+4,col:col+4] = pattern1

            img2d[0:30,:] = 0

        elif imgIdx == 3:
            matrix1 = np.array([[255, 255],
                                [255, 0]])

            matrix2 = np.array([[0, 0],
                                [0, 255]])

            pattern0 = np.hstack((matrix1, matrix2))
            pattern1 = np.hstack((matrix2, matrix1))
            for row in range(0,height,4):
                for col in range(0,width,4):
                    img2d[row:row+2,col:col+4] = pattern0
                    img2d[row+2:row+4,col:col+4] = pattern1

            img2d[0:50,:] = 0

        elif imgIdx == 4:
            # Checkerboard pattern parameters
            cell_size = 16  # Size of each checkered cell in pixels
            num_cells = height // cell_size

            # Create the checkerboard pattern
            checkerboard = np.zeros((height, width), dtype=np.uint8)

            for i in range(num_cells):
                for j in range(num_cells):
                    if (i + j) % 2 == 1:
                        checkerboard[i * cell_size: (i + 1) * cell_size, j * cell_size: (j + 1) * cell_size] = 255
            img2d = checkerboard

        elif imgIdx == 5:
            # Checkerboard pattern parameters
            cell_size = 8  # Size of each checkered cell in pixels
            num_cells = height // cell_size

            # Create the checkerboard pattern
            checkerboard = np.zeros((height, width), dtype=np.uint8)

            for i in range(num_cells):
                for j in range(num_cells):
                    if (i + j) % 2 == 1:
                        checkerboard[i * cell_size: (i + 1) * cell_size, j * cell_size: (j + 1) * cell_size] = 255
            img2d = checkerboard

        elif imgIdx == 6:
            # Checkerboard pattern parameters
            cell_size = 4  # Size of each checkered cell in pixels
            num_cells = height // cell_size

            # Create the checkerboard pattern
            checkerboard = np.zeros((height, width), dtype=np.uint8)

            for i in range(num_cells):
                for j in range(num_cells):
                    if (i + j) % 2 == 1:
                        checkerboard[i * cell_size: (i + 1) * cell_size, j * cell_size: (j + 1) * cell_size] = 255
            img2d = checkerboard

        elif imgIdx == 7:
            # Checkerboard pattern parameters
            cell_size = 1  # Size of each checkered cell in pixels
            num_cells = height // cell_size

            # Create the checkerboard pattern
            checkerboard = np.zeros((height, width), dtype=np.uint8)

            for i in range(num_cells):
                for j in range(num_cells):
                    if (i + j) % 2 == 1:
                        checkerboard[i * cell_size: (i + 1) * cell_size, j * cell_size: (j + 1) * cell_size] = 255
            img2d = checkerboard
        else:
            print(f"Image type {imgIdx} not found. Skipping to next.")
            continue

        temp_img = img2d.ravel()

        width_h = np.uint8(width // (2**8))  # get highbits: shifting right by 8 bits
        width_l = np.uint8(width % (2**8))  # get lowbits: masking with 0xFF
        height_h = np.uint8(height // (2**8))  # get highbits: shifting right by 8 bits
        height_l = np.uint8(height % (2**8))  # get lowbits: masking with 0xFF

        imgWithHeader = np.array([width_l, width_h, height_l, height_h])  # little endian
        imgWithHeader = np.append(imgWithHeader, temp_img)

        if os.path.exists("images/edge_cases/bayerCFA_GB") == False:
        # if os.path.exists("/bayerCFA_GB") == False:
            os.mkdir("images/edge_cases/bayerCFA_GB")
            # os.mkdir("/bayerCFA_GB")

        imgWithHeader.astype("uint8").tofile(
            f"images/edge_cases/bayerCFA_GB/img_{imgIdx:02}.bin"
            # f"/bayerCFA_GB/img_{imgIdx:02}.bin"
        )


        w = png.Writer(width=width, height=height, greyscale=True, bitdepth=1)
        f = open(f"/bayerCFA_GB/img_{imgIdx:02}.png", "wb")
        imgForPng = img2d.astype(np.uint8).flatten()
        imgForPng = imgForPng.reshape([height, width])
        imgForPng = np.array(imgForPng, dtype= np.uint8)

        if np.any((imgForPng < 0) | (imgForPng > 255)):
            raise ValueError("Image data contains values outside the range 0-255")

        w.write(f, imgForPng) 
        f.close()

    # plt.imshow(img2d, cmap = 'gray')
    # plt.show()

if __name__ == "__main__":
    main(sys.argv)