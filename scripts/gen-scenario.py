#!/usr/bin/env python3
"""Generate scenarios/leak.test.yaml: press the request button N times, then wait
until the firmware reports that it handled all N requests (N must be a multiple of 50,
the firmware's progress interval).

Scenarios have no loops, so the presses are written out one by one.

Usage: scripts/gen-scenario.py [presses] > scenarios/leak.test.yaml
"""

import sys

presses = int(sys.argv[1]) if len(sys.argv) > 1 else 200
if presses <= 0 or presses % 50:
    sys.exit("presses must be a positive multiple of 50")

print(f"""name: 'Leak test: {presses} requests'
version: 1
author: 'Wokwi'

steps:
  - wait-serial: 'app: ready'""")
for i in range(1, presses + 1):
    print(f"""  # request {i}
  - set-control:
      part-id: btn1
      control: pressed
      value: 1""")
    if i == presses:
        # wait-serial only sees output that arrives after it starts, so arm it before the firmware
        # handles the last press
        print(f"  - wait-serial: 'handled {presses} requests'")
    print("""  - delay: 10ms
  - set-control:
      part-id: btn1
      control: pressed
      value: 0""")
    if i < presses:
        print("  - delay: 40ms")
