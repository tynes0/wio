"""New N02 Wio semantic checks in O0/O2 and VM; no old corpus."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import sys
import uuid

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(HERE.parent))
import measurement as m

p = argparse.ArgumentParser()
p.add_argument("--wio", type=Path, required=True)
p.add_argument("--runtime", type=Path, required=True)
p.add_argument("--cxx", required=True)
p.add_argument("--output", type=Path, required=True)
args = p.parse_args()
folder = args.output.resolve() / uuid.uuid4().hex
folder.mkdir(parents=True)
shutil.copyfile(HERE / "semantics.wio", folder / "main.wio")
os.environ["WIO_ROOT"] = str(ROOT)
report = dict(Stage="N02", Passed=False, Processes=[], Failure=None)

def checked(command):
    result = m.run_process(command, folder, 240)
    report["Processes"].append(result)
    if result["Status"] != "passed":
        report["Failure"] = str(result)
    m.write_json(folder / "results.json", report)
    if result["Status"] != "passed":
        raise RuntimeError(str(result))
    return result

checked([args.wio.resolve(), folder / "main.wio", "--emit-cpp"])
generated = folder / "main.wio.cpp"
if "Wio local append reuse" not in generated.read_text(encoding="utf-8"):
    report["Failure"] = "New backend did not select local append fusion"
    m.write_json(folder / "results.json", report)
    raise RuntimeError("New backend did not select local append fusion")
for optimization in ("-O0", "-O2"):
    program = folder / ("native.exe" if os.name == "nt" else "native")
    checked([args.cxx, "-std=c++20", optimization, str(generated), f"-I{ROOT / 'runtime/include'}",
             f"-I{ROOT / 'sdk/include'}", args.runtime.resolve(),
             *(["-lws2_32", "-lshell32"] if os.name == "nt" else ["-pthread"]), "-o", program])
    checked([program])
bridge = folder / ("bridge.dll" if os.name == "nt" else "bridge.dylib" if sys.platform == "darwin" else "bridge.so")
bytecode = folder / "main.wiob"
checked([args.wio.resolve(), folder / "main.wio", "--target", "shared", "--native-bridge-only",
         "--output", bridge, "--bytecode-sidecar", bytecode])
checked([args.wio.resolve(), "--vm-run-bytecode", bytecode, "--native-library", bridge])
report["Passed"] = True
m.write_json(folder / "results.json", report)
print("N02 append semantics: native O0/O2 and VM passed")
