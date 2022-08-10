#!/usr/bin/python3
from mkpy.utility import *

import cv2 as cv #pip install opencv-python
import matplotlib.pyplot as plt #pip install matplotlib
import matplotlib.colors #pip install matplotlib
import numpy as np

# This was an attempt at writing an image processing algorithm that would take
# images as input and detect content that's expected to rectangular in shape.
# The idea was to use it to extract pictures and receipts from scans of them.
#
# I didn't like that all algorithms I could come up with were too brittle and
# depended a lot on exact details of the images passed. I now think the right
# approach it to use machine learning to train a neural network to do this.

def img_resize(image, width = None, height = None, inter = cv.INTER_AREA):
    if width == None and height == None:
        return image

    (h, w) = image.shape[:2]

    if height != None:
        r = height/float(h)
        new_size = (int(w*r), height)

    else:
        r = width/float(w)
        new_size = (width, int(h*r))

    p_src = np.float32([[0,0],[0,h],[w,0]])
    p_tgt = np.float32([[0,0],[0,new_size[1]],[new_size[0],0]])

    return cv.resize(image, new_size, interpolation = inter), cv.getAffineTransform(p_src, p_tgt)

def transform_point (T, x, y):
    if T is not None:
        res = (x*T[0][0] + y*T[0][1] + T[0][2], x*T[1][0] + y*T[1][1] + T[1][2])
    else:
        res = (x,y)

    return tuple(map(int, res))

def draw_lines (image, lines, Tfm=None, width=1):
    res = image.copy()

    if lines is not None:
        for i in range(0, len(lines)):
            l = lines[i][0]
            cv.line(res, transform_point(Tfm, l[0], l[1]), transform_point(Tfm, l[2], l[3]), (0,0,255), width, cv.LINE_8)

    return res

def process_file():
    global images, images_img

    fname = get_cli_arg_opt ("--file")

    if fname == None:
        print (f"usage: ./outliner.py --file FILE_NAME")
        return

    src = cv.imread(fname)
    (src_h, src_w) = src.shape[:2]

    bw = cv.cvtColor(src, cv.COLOR_BGR2GRAY)

    small, T = img_resize (src, width = 50)
    Tinv = cv.invertAffineTransform(T)

    blur = cv.bilateralFilter(small, 3, 1, 1)

    canny = cv.Canny(blur, 50, 90, None, 3)

    lines = cv.HoughLinesP(canny, 1, np.pi / 180, 10, maxLineGap = 10)

    res_small = draw_lines (cv.cvtColor(canny, cv.COLOR_GRAY2RGB), lines)
    res = draw_lines (cv.cvtColor(src, cv.COLOR_BGR2RGB), lines, Tfm=Tinv, width=3)

    plt.imshow(res)
    plt.show()

process_file()
