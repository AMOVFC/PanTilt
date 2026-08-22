import subprocess, sys, os
PY = sys.executable
KPY = r"C:\Program Files\KiCad\9.0\bin\python.exe"
for cmd in ([PY,"gen_sch.py"], [PY,"export_design.py"], [KPY,"build_pcb.py"],
            [PY,"gen_sat.py"], [KPY,"build_pcb_sat.py"]):
    r = subprocess.run(cmd, capture_output=True, text=True)
    tail = [l for l in r.stdout.strip().split("\n") if l][-1:] 
    print(("OK  " if r.returncode==0 else "FAIL"), cmd[1], "|", tail[0] if tail else "", flush=True)
    if r.returncode: print(r.stdout[-800:], r.stderr[-800:]); sys.exit(1)
