import subprocess
import time
import csv
import re
import sys
import socket

pattern = re.compile(r"\[(\d+)\]ns/iter")

results = []

args = ["./GemmASM.exe", "-method=0", "-incache", "-perfonly"]

allmethod = [0, 3]
rangemax = 4096

for arg in sys.argv[1:]:
    if arg.startswith("-method="):
        allmethod = [int(x) for x in arg[8:].split(",")]
    elif arg.startswith("-testrange="):
        rangemax = int(arg[11:])
    else:
        args.append(arg)

args.append("-dims=")


tests = [(512, 512), (1024, 1024), (1536, 1536)]
for i in range(2048, rangemax, 512):
    tests.append((1536, i))
    tests.append((i, 1536))

for method in [0, 2, 3]:
    args[1] = f"-method={method}"
    for incache in [True, False]:
        args[2] = "-incache" if incache else "-dummy"
        for n,k in tests:
            args[-1] = f"-dims={n}x{k}"
            print(f"Test m[{method}]({'Y' if incache else 'N'}): {n}x{k}")
            result = subprocess.run(args, capture_output=True, text=True, timeout=10, check=False)
            output = result.stdout
            match = pattern.search(output)
            r = int(match.group(1))
            results.append((n,k,method,"cache" if incache else "ram",r))
            time.sleep(1)

with open(f"results-{socket.gethostname()}.csv", "w", newline='', encoding="utf8") as f:
    writer = csv.writer(f)
    writer.writerow(["n", "k", "method", "cache", "time"])
    writer.writerows(results)