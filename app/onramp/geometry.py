import xml.etree.ElementTree as ET


class Bounds:
    """ Iteratively construct bounding box for a series of (x, y) points. """

    def __init__(self):
        self.minx = 180
        self.maxx = -180
        self.miny = 90
        self.maxy = -90

    def add(self, x, y):
        if x < self.minx:
            self.minx = x
        if x > self.maxx:
            self.maxx = x
        if y < self.miny:
            self.miny = y
        if y > self.maxy:
            self.maxy = y

    def elem(self):
        e = ET.Element("bounds")
        e.set("minlat", "{:.07f}".format(self.miny))
        e.set("minlon", "{:.07f}".format(self.minx))
        e.set("maxlat", "{:.07f}".format(self.maxy))
        e.set("maxlon", "{:.07f}".format(self.maxx))
        return e
