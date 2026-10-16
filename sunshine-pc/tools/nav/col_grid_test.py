"""Deterministic .col-to-.nav regressions; no disc assets or native build required.

Usage: python tools/nav/col_grid_test.py
"""
import os
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

import col_grid


def make_col(vertices, faces):
    vertex_offset = 16
    group_offset = vertex_offset + len(vertices) * 12
    index_offset = group_offset + 24
    return (struct.pack('>4I', len(vertices), vertex_offset, 1, group_offset)
            + b''.join(struct.pack('>3f', *v) for v in vertices)
            + struct.pack('>4H4I', 0, len(faces), 0, 0, index_offset, 0, 0, 0)
            + b''.join(struct.pack('>3H', *f) for f in faces))


def generate_grid(vertices, faces):
    with tempfile.TemporaryDirectory() as directory:
        out = os.path.join(directory, 'fixture.nav')
        with patch.object(col_grid, 'load_szs', return_value={'/map/map.col': make_col(vertices, faces)}), \
                patch.object(sys, 'argv', ['col_grid.py', 'fixture', out, '-1', '301']):
            col_grid.main()
        with open(out, 'rb') as f:
            data = f.read()
        return {(x, z): y for x, z, y in struct.iter_unpack('<hhh', data)}


class ColGridTest(unittest.TestCase):
    def test_ramp_has_no_artificial_triangle_seam(self):
        grid = generate_grid([(0, 0, 0), (0, 300, 400), (240, 300, 400), (240, 0, 0)],
                             [(0, 1, 2), (0, 2, 3)])
        # This plane is y = 0.75*z; the consumer samples 40-unit cell centres.
        self.assertEqual([grid[2, z] for z in range(10)], list(range(15, 286, 30)))
        # navSafe rejects a 45-unit jump; centroid heights used to jump 100 at this seam.
        self.assertLess(abs(grid[2, 4] - grid[2, 3]), 45)

    def test_downward_ceiling_does_not_replace_ground(self):
        grid = generate_grid([(0, 20, 0), (0, 20, 400), (240, 20, 400), (240, 20, 0),
                              (0, 100, 0), (240, 100, 0), (0, 100, 400)],
                             [(0, 1, 2), (0, 2, 3), (4, 5, 6)])
        self.assertEqual(grid[0, 0], 20)
        self.assertEqual(set(grid.values()), {20})


if __name__ == '__main__':
    unittest.main()
