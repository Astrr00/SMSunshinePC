#!/usr/bin/env python3
"""Check status inputs and SVG output without a build report or game data."""
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

import status_treemap as status

NS = {"s": "http://www.w3.org/2000/svg"}


class StatusGraphicsTest(unittest.TestCase):
    def test_matrix_rejects_misleading_counts(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "matrix.tsv"
            for body in ["", "PC-A\tA\tpassed\n", "PC-A\t\topen\n",
                         "PC-A\tA\topen\nPC-A\tB\tpassed-bounded\n"]:
                with self.subTest(body=body):
                    path.write_text("id\tarea\tstatus\n" + body, encoding="utf-8")
                    with self.assertRaises(ValueError):
                        status.port_rows(path)
            path.write_text("id\tarea\tstatus\nPC-A\tA\topen\n", encoding="utf-8")
            self.assertEqual(len(status.port_rows(path)), 1)

    def test_pending_checks_are_visible_and_escaped(self):
        rows = [{"id": "PC-A", "area": "Water & effects <test>", "status": "open"},
                {"id": "PC-B", "area": "Clean Windows", "status": "external-check-required"},
                {"id": "PC-C", "area": "Boot", "status": "passed-bounded"}]
        root = ET.fromstring(status.port_graphic(rows))
        visible = [node.text for node in root.findall("s:text", NS)]
        self.assertIn("Water & effects <test>", visible)
        self.assertIn("Clean Windows", visible)
        self.assertIn("1 of 3 checks passed", root.find("s:desc", NS).text)
        self.assertIn("Not overall completion", root.find("s:desc", NS).text)

    def test_matching_keeps_function_and_byte_measures_separate(self):
        report = {"measures": {"matched_functions": 1, "total_functions": 2,
                               "matched_code": 3, "total_code": 12,
                               "complete_units": 0, "total_units": 1},
                  "units": [{"name": "src/Water/A&B.cpp", "measures": {
                      "total_functions": 2, "matched_functions": 1}}]}
        root = ET.fromstring(status.matching_graphic(report))
        visible = [node.text for node in root.findall("s:text", NS)]
        self.assertIn("50.0%", visible)
        self.assertIn("25.00%", visible)
        self.assertIn("0 / 1", visible)
        self.assertIn("A&B.cpp", root.find("s:g/s:title", NS).text)
        report["units"][0]["measures"]["matched_functions"] = 3
        with self.assertRaises(ValueError):
            status.matching_graphic(report)
        report["measures"]["total_code"] = 0
        with self.assertRaises(ValueError):
            status.matching_graphic(report)

    def test_treemap_preserves_area_and_bounds(self):
        for weights in ([0], [1], [1000, 3, 2, 1, 0], [1] * 736):
            with self.subTest(weights=weights[:5]):
                cells = status.squarify([(n, i) for i, n in enumerate(weights)], 0, 0, 936, 420)
                self.assertEqual(len(cells), sum(n > 0 for n in weights))
                self.assertAlmostEqual(sum(w*h for _, _, _, w, h in cells), 936*420 if any(weights) else 0)
                for _, x, y, w, h in cells:
                    self.assertGreaterEqual(min(x, y, w, h), -1e-8)
                    self.assertLessEqual(x+w, 936+1e-8)
                    self.assertLessEqual(y+h, 420+1e-8)


if __name__ == "__main__":
    unittest.main()
