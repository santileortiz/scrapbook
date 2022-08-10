from mkpy.utility import *

import math, functools
from collections import namedtuple
from itertools import islice

class Point(namedtuple('Point', 'x y')):
    def __str__(self):
        return f'Point({self.x},{self.y})'

class Segment(namedtuple('Segment', 'p1 p2')):
    def __str__(self):
        return f'({self.p1}) - ({self.p2})'

def points_area2(a, b, c):
    """
    Compute twice the area of the triangle spanned by the 3 points passed.
    """
    return (b.x - a.x)*(c.y - a.y) - (c.x - a.x)*(b.y - a.y)

def points_left (a, b, c):
    """
    Returns True if point C is to the left of the segment AB, False otherwise.
    """
    return points_area2 (a, b, c) > 0

def points_lefton (a, b, c):
    """
    Returns True if point C is on the segment AB or to the left of AB, False otherwise.
    """
    return points_area2 (a, b, c) > 0

@automatic_test_function(permute_args=True)
def points_collinear (a, b, c):
    """
    Returns True if the 3 points are collinear.
    """
    return points_area2 (a, b, c) == 0

def points_between (a, b, c):
    """
    Returns True if point C lies in the segment AB.
    """
    # C can't be between AB unless A, B and C are collinear.
    if not points_collinear(a,b,c):
        return False

    # Segment AB is not vertical
    if a.x != b.x:
        return (a.x <= c.x and c.x <= b.x) or (a.x >= c.x and c.x >= b.x) 
    else:
        return (a.y <= c.y and c.y <= b.y) or (a.y >= c.y and c.y >= b.y) 

def points_intersect (a, b, c, d, proper=False):
    """
    Returns True if the segments AB and CD have an intersection.

    A proper intersection between segments AB and CD is a point in AB and CD
    but different to A, B, C and D. To check for the existence of a proper
    intersection set proper=True.
    """

    if proper:
        # If any 3 points are collinear we can't have a proper intersection
        # between the segments.
        if points_collinear(a, b, c) or points_collinear (a, b, d) or points_collinear (c, d, a) or points_collinear (c, d, b):
            return False

        return points_left (a, b, c) != points_left (a, b, d) or points_left (c, d, a) != points_left (c, d, b)

    else:
        if points_intersect (a, b, c, d, proper=True):
            return True

        elif points_between(a, b, c) or points_between (a, b, d) or points_between (c, d, a) or points_between (c, d, b):
            return True

        else:
            return False

@automatic_test_function(permute_args=True)
def segments_intersect (s1, s2):
    return points_intersect (s1.p1, s1.p2, s2.p1, s2.p2)

def polygon_line_set (points):
    """
    Returns a the set of lines corresponding to the passed polygon. A polygon
    is represented as a list of points.
    """

    lines = set()

    prev_point = points[0]
    for point in islice(points, 1):
        lines.add (Segment(prev_point, point))
        prev_point = point
    lines.add (Segment(prev_point, points[0]))

    return lines

@automatic_test_function
def points_centroid (points):
    x_sum = 0
    y_sum = 0
    for p in points:
        x_sum += p.x
        y_sum += p.y

    return Point(x_sum/len(points), y_sum/len(points))

@automatic_test_function
def points_min (points):
    result = None
    dist = math.inf
    for p in points:
        new_dist = p.x + p.y
        if new_dist < dist:
            result = p
            dist = new_dist

    return result

@automatic_test_function
def point_get_quadrant(p, center):
    dx = p.x - center.x
    dy = p.y - center.y

    if dy >= 0 and dx > 0:
        quadrant = 1
    elif dx <= 0 and dy > 0:
        quadrant = 2
    elif dy <= 0 and dx < 0:
        quadrant = 3
    else:
        quadrant = 4

    return quadrant

@automatic_test_function(permute_args=0)
def points_sort (points, center, reverse=False):
    def point_compare(p1, p2):
        p1_q = point_get_quadrant (p1, center)
        p2_q = point_get_quadrant (p2, center)

        if p1_q != p2_q:
            return p1_q - p2_q
        else:
            return points_area2 (center, p2, p1)

    return sorted(points, key=functools.cmp_to_key(point_compare), reverse=reverse)

@automatic_test_function(permute_args=0)
def points_sortk (points, center, reverse=False):
    def key(p):
        # This version "unrolls" the halfplane for y < 0, then sorts by the x
        # coordinate of points. It has fewer ifs because it doens't compute the
        # quadrant of the point and doesn't split the cases of comparing points
        # in the same quadrant and in diferent one. The cost of this is having
        # to compute a square root.
        # TODO: Even though this passes the test, I'm not conviced about the
        # derivation...
        dx = p.x - center.x
        dy = p.y - center.y
        return math.sqrt(dx**2+dy**2) - dx

    return sorted(points, key=key, reverse=reverse)
