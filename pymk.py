#!/usr/bin/python3
from mkpy.utility import *
from geometry import *

import time
import numpy as np
from matplotlib.transforms import Affine2D
from PIL import ImageShow, Image
import inspect
import json

modes = {
        'debug': '-O0 -g -Wall',
        'profile_debug': '-O3 -g -pg -Wall',
        'release': '-O3 -g -DNDEBUG -Wall'
        }
mode = store('mode', get_cli_arg_opt('-M,--mode', modes.keys()), 'debug')
C_FLAGS = modes[mode]

ensure_dir('bin')

def default ():
    target = store_get('last_snip', default='scrapbook')
    call_user_function(target)

def scrapbook ():
    ex(f'gcc {C_FLAGS} -o bin/scrapbook scrapbook.c -mavx -maes -lm')

class XdgViewer (ImageShow.UnixViewer):
    def get_command_ex(self, file, **options):
        command = executable = "gpicview"
        return command, executable
ImageShow.register(XdgViewer)

def transform_as_tuple(transform):
    matrix = transform.get_matrix()

    a = matrix[0,0]
    b = matrix[0,1]
    c = matrix[0,2]
    d = matrix[1,0]
    e = matrix[1,1]
    f = matrix[1,2]

    return (a, b, c, d, e, f)

def transform_point (T, x, y):
    if T is not None:
        res = (x*T[0] + y*T[1] + T[2], x*T[3] + y*T[4] + T[5])
    else:
        res = (x,y)

    return tuple(map(int, res))

def perform_outline_crop ():
    label_directory = None
    label_directory_cli = get_cli_arg_opt ("--label-directory")
    if label_directory_cli != None:
        label_directory = os.path.abspath (label_directory_cli)

    image_directory = None
    no_opt_args = get_cli_no_opt ()
    if no_opt_args != None and len(no_opt_args) > 0:
        image_directory = os.path.abspath (no_opt_args[0])

    # Support label_directory == None in which case we jus look for the label
    # file in the same directory as the image.
    if image_directory == None or label_directory == None:
        print (f"usage: ./pymk.py {get_function_name()} [--label-directory LABEL_DIRECTORY] DIRECTORY")
        return

    label_files = {}
    for dirpath, dirnames, filenames in os.walk(label_directory):
        for fname in filenames:
            f_base, f_extension = path_split (fname)

            f_base
            if f_extension == ".json":
                label_files[f_base] = path_cat(dirpath, fname)

    files = {}
    for dirpath, dirnames, filenames in os.walk(image_directory):
        for fname in filenames:
            f_base, f_extension = path_split (fname)

            if f_base in label_files.keys():
                output_path = path_cat(dirpath, fname)
                original_path = path_cat(label_directory, fname)

                if not path_exists(original_path):
                    print (f'Created image backup from current version: {original_path}')
                    shutil.copy(output_path, original_path)

                files[f_base] = (output_path, original_path, label_files[f_base])

            else:
                print (ecma_red ("error:") + f" Skipped image because there is no outline label file: {path}")

    for f_id in files:
        output_path, original_path, label_path = files[f_id]

        # Array that will contain coordinates for the 4 corners of the
        # outline, starting at the top left and in clockwise direction.
        label_outline = None
        with open (label_path) as f:
            labels_json = json.load (f)

        # This is specific to the format used in labelme
        #
        # TODO: Should I check the version number of the file?, how often
        # will it change? are versions guaranteed to be backward
        # compatible?, currently it seems to use the program's version
        # which I don't really like much, I hope the label format doesn't
        # change with each release?.

        for shape in labels_json['shapes']:
            if shape['label'] == 'outline':
                if shape['shape_type'] == 'polygon' and shape['label'] == 'outline':
                    label_outline = shape['points']
                else:
                    print (ecma_red ("error:") + f" Outline is not of polygon type.")

        if label_outline == None:
            print (ecma_red ("error:") + f" Skipped image because an 'outline' label could not be loaded: {label_path}")
            continue

        outline = [Point(p[0], p[1]) for p in label_outline]
        centroid = points_centroid (outline)
        sorted_outline = points_sort (outline, centroid)



        # Compute affine transformation that translates the top border to the
        # top edge of the image. This doesn't distort the image because we
        # assume it was scanned with a proper scanner (i.e. it's not a picture
        # to which we need to perform perspective correction).
        top_left = sorted_outline[2]
        top_right = sorted_outline[3]
        bottom_left = sorted_outline[1]

        angle = np.arctan2 (top_right.y - top_left.y, top_right.x - top_left.x)
        width = round(math.sqrt((top_right.y - top_left.y)**2 + (top_right.x - top_left.x)**2))
        height = round(math.sqrt((bottom_left.y - top_left.y)**2 + (bottom_left.x - top_left.x)**2))
        transform = Affine2D().translate(top_left.x, top_left.y).rotate_around(top_left.x, top_left.y, angle)



        # Compute the cropping coordinates to have a rectangle within the
        # bounds of the transformed outline.
        inverted_transform = transform_as_tuple(transform.inverted())
        transformed_outline = [transform_point(inverted_transform, p.x, p.y) for p in sorted_outline]

        error = 1
        crop_top = error
        crop_bottom = min(transformed_outline[0][1], transformed_outline[1][1]) - error
        crop_left = max(transformed_outline[1][0], transformed_outline[2][0]) + error
        crop_right = min(transformed_outline[3][0], transformed_outline[0][0]) - error

        with Image.open(original_path) as im:
            transformed_image = im.transform((width, height), Image.AFFINE, transform_as_tuple(transform), Image.BICUBIC)
            cropped_image = transformed_image.crop((crop_left, crop_top, crop_right, crop_bottom))

            cropped_image.save(output_path)
            #ImageShow.show(output_image)

        print (f'{output_path}')

def get_image_size(path):
    jpg_x = ex(f"exiv2 pr '{path}' | awk -F: '/Image size/ {{print $2}}' | cut -dx -f1 | tr -d ' '", ret_stdout=True, echo=False)
    jpg_y = ex(f"exiv2 pr '{path}' | awk -F: '/Image size/ {{print $2}}' | cut -dx -f2 | tr -d ' '", ret_stdout=True, echo=False)

    return (jpg_x, jpg_y)


def show_orientation_info():
    path = None
    no_opt_args = get_cli_no_opt ()
    if no_opt_args != None and len(no_opt_args) > 0:
        path = os.path.abspath (no_opt_args[0])

    if path == None:
        print (f"usage: ./pymk.py {get_function_name()} FILE")
        return

    jpg_x, jpg_y = get_image_size(path)

    print (f'JPG.X\t{jpg_x}')
    print (f'JPG.Y\t{jpg_y}')

    keys = ['Exif.Photo.PixelXDimension', 'Exif.Photo.PixelYDimension', 'Exif.Image.ImageWidth', 'Exif.Image.ImageLength', 'Xmp.exif.PixelXDimension', 'Xmp.exif.PixelYDimension', 'Xmp.tiff.ImageWidth', 'Xmp.tiff.ImageHeight']

    for k in keys:
        value = ex (f"exiv2 -K'{k}' '{path}' | awk '{{print $4}}'", ret_stdout=True, echo=False)
        print (f'{k}\t{value if len(value)>0 else "-"}')

def rotate_cw():
    image_list = None
    no_opt_args = get_cli_no_opt ()
    if no_opt_args != None and len(no_opt_args) > 0:
        image_list = [os.path.abspath(p) for p in no_opt_args]

    if image_list == None:
        print (f"usage: ./pymk.py {get_function_name()} FILES...")
        return

    print (image_list)
    for path in image_list:
        ex (f"exiv2 -M'set Exif.Image.Orientation 6' mo '{path}'")
        ex (f"exiv2 -d t '{path}'")
        ex (f"convert '{path}' -auto-orient '{path}'")

    ex (f"rm -r ~/.cache/thumbnails")

def rotate_ccw():
    image_list = None
    no_opt_args = get_cli_no_opt ()
    if no_opt_args != None and len(no_opt_args) > 0:
        image_list = [os.path.abspath(p) for p in no_opt_args]

    if image_list == None:
        print (f"usage: ./pymk.py {get_function_name()} FILES...")
        return

    print (image_list)
    for path in image_list:
        ex (f"exiv2 -M'set Exif.Image.Orientation 8' mo '{path}'")
        ex (f"exiv2 -d t '{path}'")
        ex (f"convert '{path}' -auto-orient '{path}'")

    ex (f"rm -r ~/.cache/thumbnails")


if __name__ == "__main__":
    # Everything above this line will be executed for each TAB press.
    # If --get_completions is set, handle_tab_complete() calls exit().
    handle_tab_complete()

    pymk_default()

