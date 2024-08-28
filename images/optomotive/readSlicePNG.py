
# Reads PNG image, then cuts it to smaller images and saves pixel values in binary.

import png
import numpy as np
from skimage import measure
import os

def calc_entropy(data, minVal, maxVal):
    bins = range(minVal, maxVal+1)
    freq, bins = np.histogram(data, bins = bins) #, range = (0,2048+1))
    freq = filter(lambda p: p > 0, freq)
    freq = list(freq)
    prob = freq/sum(freq)
    entropy_bayer_manual = -sum(np.multiply(prob, np.log2(prob)))
    return entropy_bayer_manual

try:    
    r = png.Reader("Smilodon_GMAX2518_BASLERC11-1620-12M_BAYER8GB.png")
except:
    r = png.Reader("images\optomotive\Smilodon_GMAX2518_BASLERC11-1620-12M_BAYER8GB.png")

img = r.read()

width = img[0]
height = img[1]
pngdata = img[2]

print(img)
print("width:")
print(width)
print("height:")
print(height)

# img2d = np.vstack(map(np.uint8, pngdata)) # will be deprecatd
img2d = np.vstack([np.uint8(row) for row in pngdata])
#img2d = np.empty((height, width), dtype=np.uint8) # slower
#for i, el in enumerate(pngdata):
#    # print(len(list(map(np.uint8, el))))
#    img2d[i, :] = list(map(np.uint8, el))

print(img2d.shape)
print("-----------")

# FULL IMAGE FIRST (idx = 30)
temp_img = img2d
temp_img = temp_img.ravel()
# entropy = measure.shannon_entropy(temp_img)
entropy = calc_entropy(temp_img, 0, 255)
print(f"Full image : shape = {temp_img.shape}, entropy = {entropy:0.2f}")
img_size_x = np.uint16(width)
img_size_y = np.uint16(height)
# width_h = np.uint8(img_size_x//(2**8)) # get highbits: shifting right by 8 bits
# width_l = np.uint8(img_size_x%(2**8)) # get lowbits: masking with 0xFF
# height_h = np.uint8(img_size_y//(2**8)) # get highbits: shifting right by 8 bits
# height_l = np.uint8(img_size_y%(2**8)) # get lowbits: masking with 0xFF

idx = 30
w = png.Writer(width=width, height=height, greyscale=True, bitdepth=8)
if os.path.exists("original") == False:
    os.mkdir("original")
    
f = open(f"original/img_{idx:02}.png", "wb")
imgForPng = img2d.astype(np.uint8).flatten()
imgForPng = imgForPng.reshape([img_size_y, img_size_x])
w.write(f, imgForPng)
f.close()

# imgWithHeader = np.array([width_l,width_h,height_l,height_h]) # little endian
# imgWithHeader = np.append(imgWithHeader,temp_img)
# imgWithHeader.astype('uint8').tofile(f"bayerCFA_GB/img_{idx:02}.bin")

# cut image

# img_size_x = np.uint16(width/4)
img_size_x = np.uint16(1120)
img_size_y = np.uint16(height/4)
# x = ((0,1127),(1128,2255),(2256,3383),(3384,4511))
x = ((0,1119),(1120,2239),(2240,3359),(3360,4479))
y = ((0,1023),(1024,2047),(2048,3071),(3072,4095))

for j in range(0,len(y),1):
    for i in range(0,len(x),1):
        idx = i+j*len(x)
        x_s = x[i][0]
        x_e = x[i][1]+1
        y_s = y[j][0]
        y_e = y[j][1]+1
        temp_img = img2d[y_s : y_e, x_s : x_e]

        w = png.Writer(width=img_size_x, height=img_size_y, greyscale=True, bitdepth=8)
        f = open(f"original/img_{idx:02}.png", "wb")
        imgForPng = temp_img.astype(np.uint8).flatten()
        imgForPng = imgForPng.reshape([img_size_y, img_size_x])
        w.write(f, imgForPng)
        f.close()

        temp_img = temp_img.ravel()
        # entropy = measure.shannon_entropy(temp_img)
        entropy = calc_entropy(temp_img, 0, 255)
        print(f"Image nr {idx:02}: shape = {temp_img.shape}, entropy = {entropy:0.2f}")
        # width_h = np.uint8(img_size_x//(2**8)) # get highbits: shifting right by 8 bits
        # width_l = np.uint8(img_size_x%(2**8)) # get lowbits: masking with 0xFF
        # height_h = np.uint8(img_size_y//(2**8)) # get highbits: shifting right by 8 bits
        # height_l = np.uint8(img_size_y%(2**8)) # get lowbits: masking with 0xFF

        # imgWithHeader = np.array([width_l,width_h,height_l,height_h]) # little endian
        # imgWithHeader = np.append(imgWithHeader,temp_img)
        # imgWithHeader.astype('uint8').tofile(f"images/optomotive/original/img_{idx:02}.bin")





