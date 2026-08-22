import subprocess, sys
PY = sys.executable
KPY = r"C:\Program Files\KiCad\9.0\bin\python.exe"
# AS5600s are off-the-shelf breakout modules wired in via a 4-pin XH header,
# so there is no second board to build -- just the main controller.
for cmd in ([PY,"gen_sch.py"], [PY,"export_design.py"], [KPY,"build_pcb.py"]):
    r = subprocess.run(cmd, capture_output=True, text=True)
    tail = [l for l in r.stdout.strip().split("\n") if l][-1:]
    print(("OK  " if r.returncode==0 else "FAIL"), cmd[1], "|", tail[0] if tail else "", flush=True)
    if r.returncode:
        print(r.stdout[-900:], r.stderr[-900:]); sys.exit(1)
