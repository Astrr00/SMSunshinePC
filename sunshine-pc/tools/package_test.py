#!/usr/bin/env python3
"""Disc-image exclusion regressions: python tools/package_test.py (stdlib only)."""
import unittest

from package import banned


class PackageDataTest(unittest.TestCase):
    def test_compressed_disc_images_are_rejected_among_port_assets(self):
        paths = [
            "assets/launcher/ocean.png",
            "assets/accidental.WIA",
            "assets/nested/accidental.gCz",
            "licenses/nod-LICENSE-MIT.txt",
        ]
        self.assertEqual(banned(paths), paths[1:3])


if __name__ == "__main__":
    unittest.main()
