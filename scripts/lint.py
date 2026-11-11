#!/usr/bin/env python3
"""Run Radiant-owned policy rules with audited inline suppressions."""

import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rule", default=".", help="regular expression selecting rule IDs")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    try:
        selected = re.compile(args.rule)
    except re.error as error:
        parser.error(str(error))
    rules = [path for path in sorted(Path(__file__).with_name("lint_rules").glob("*.yml"))
             if selected.search(path.stem)]
    if not rules:
        parser.error("no policy rules match the selector")
    executable = shutil.which("ast-grep")
    if not executable:
        parser.error("ast-grep is required (brew install ast-grep)")
    root = args.root.resolve()
    findings = 0
    checked = 0
    for rule in rules:
        result = subprocess.run([executable, "scan", "--rule", str(rule.resolve()),
                                 "--json=stream", "radiant"], cwd=root,
                                capture_output=True, text=True)
        if result.returncode not in (0, 1):
            sys.stderr.write(result.stderr)
            return result.returncode
        for record in result.stdout.splitlines():
            finding = json.loads(record)
            checked += 1
            line = finding["range"]["start"]["line"]
            source = (root / finding["file"]).read_text().splitlines()[line]
            # a reason on the cast's own line makes the integer exception reviewable.
            if re.search(r"//.*\bINT_CAST_OK:\s*\S", source):
                continue
            findings += 1
            print(f'{finding["file"]}:{line + 1}: {finding["ruleId"]}: {finding["message"]}')
    print(f"lint: {len(rules)} rule(s), {checked} cast(s) checked, {findings} unmarked")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
