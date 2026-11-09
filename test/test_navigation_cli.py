#!/usr/bin/env python3
"""RI-003: exercise startup navigation in the layout and view hosts."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import unittest


ROOT = Path(__file__).resolve().parent.parent
OPTIONS = None


class NavigationCliTest(unittest.TestCase):
    def setUp(self):
        self.work = ROOT / "temp" / "navigation-cli" / self._testMethodName
        self.work.mkdir(parents=True, exist_ok=True)
        self.env = dict(os.environ)
        self.env.update({
            "LAMBDA_HOME": str(OPTIONS.lambda_root / "lmd"),
            "LAMBDA_TEMP_DIR": str(self.work),
            "LAMBDA_CACHE_DIR": str(self.work / "cache"),
            "LAMBDA_LOG_DIR": str(self.work),
        })

    def page(self, navigation, name="source.html"):
        path = self.work / name
        path.write_text("""<!doctype html><html><body><div id="result" data-document="%s"></div><script>
            setTimeout(function(){
                var result=document.getElementById('result');
                result.setAttribute('data-timer','yes');
                result.textContent='timer ran';
            },0);
            %s
            </script></body></html>""" % (name, navigation), encoding="utf-8")
        return path

    def run_host(self, command, name, host=None):
        log = self.work / (name + ".log")
        with log.open("w") as output:
            result = subprocess.run([str(host or OPTIONS.host)] + command, cwd=ROOT,
                                    env=self.env, stdout=output,
                                    stderr=subprocess.STDOUT, timeout=30)
        self.assertEqual(result.returncode, 0, "CLI failed; see %s" % log)

    def replay(self, page, expected_document="source.html"):
        events = self.work / "events.json"
        result = self.work / "result.json"
        events.write_text(json.dumps({"events": [{
            "type": "assert_attribute", "target": {"selector": "#result"},
            "attribute": "data-timer", "equals": "yes"
        }, {
            "type": "assert_attribute", "target": {"selector": "#result"},
            "attribute": "data-document", "equals": expected_document
        }]}), encoding="utf-8")
        result.unlink(missing_ok=True)
        self.run_host(["view", str(page), "--headless", "--event-file", str(events),
                       "--event-result", str(result)], "view")
        report = json.loads(result.read_text(encoding="utf-8"))
        self.assertEqual(report["result"], "PASS", report)
        self.assertEqual(report["assertions"], {"passed": 2, "failed": 0}, report)

    def test_view_relative_fragment_retains_timer(self):
        self.replay(self.page("location.replace('#result');"))

    def test_view_absolute_fragment_retains_timer(self):
        self.replay(self.page("location.replace(location.href.split('#')[0]+'#result');"))

    def test_view_repeated_fragment_retains_timer(self):
        self.page("location.replace('#result');", "target.html")
        self.replay(self.page("location.replace('target.html#result');"), "target.html")

    def test_view_empty_fragment_retains_timer(self):
        self.replay(self.page("location.replace('#');"))

    def test_view_cross_document_still_redirects(self):
        self.page("", "target.html")
        self.replay(self.page("location.replace('target.html#result');"), "target.html")

    def test_view_query_change_still_redirects(self):
        self.replay(self.page("""if(location.href.indexOf('?next=1')<0){
            location.replace('?next=1#result');
        }"""))

    def test_layout_relative_fragment_retains_timer(self):
        self.check_layout("location.replace('#result');")

    def test_layout_absolute_fragment_retains_timer(self):
        self.check_layout("location.replace(location.href.split('#')[0]+'#result');")

    def check_layout(self, navigation):
        page = self.page(navigation)
        output = self.work / "layout.json"
        output.unlink(missing_ok=True)
        self.run_host(["layout", str(page), "--view-output", str(output)], "layout",
                      OPTIONS.layout_host)
        tree = json.loads(output.read_text(encoding="utf-8"))
        self.assertIn("timer ran", json.dumps(tree))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", type=Path, default=ROOT.parent / "Lambda" / "lambda.exe")
    parser.add_argument("--layout-host", type=Path, default=ROOT / "radiant.exe")
    parser.add_argument("--lambda-root", type=Path, default=ROOT.parent / "Lambda")
    OPTIONS, remaining = parser.parse_known_args()
    OPTIONS.host = OPTIONS.host.resolve()
    OPTIONS.layout_host = OPTIONS.layout_host.resolve()
    OPTIONS.lambda_root = OPTIONS.lambda_root.resolve()
    unittest.main(argv=[__file__] + remaining)
