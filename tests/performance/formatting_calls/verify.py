"""New-only scalar formatting-call gate: WIR dispatch, native O0/O2 and VM."""
import argparse
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def verify(args):
    folder = args.output.resolve() / uuid.uuid4().hex
    folder.mkdir(parents=True)
    shutil.copyfile(HERE / "semantics.wio", folder / "main.wio")
    rows = []

    def run(command, expected=0):
        row = dict(command=[str(item) for item in command], expectedExitCode=expected)
        rows.append(row)
        result = subprocess.run(row["command"], cwd=folder, env=dict(os.environ, WIO_ROOT=str(ROOT)),
                                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=300)
        row.update(exitCode=result.returncode, stdout=result.stdout, stderr=result.stderr)
        (folder / "results.json").write_text(json.dumps(dict(passed=False, processes=rows), indent=2) + "\n")
        if result.returncode != expected:
            raise RuntimeError(str(row))
        return result

    wio = args.wio.resolve()
    lowered = folder / "calls.lwir"
    run([wio, folder / "main.wio", "--emit-lowered-wir", "--ir-output", lowered])
    ir = lowered.read_text(encoding="utf-8")
    for required in ('"format.null"', '"format.call"', '"format.merge"', '::"ToString"',
                     "interface-call", "std::ToString$specialized"):
        if required not in ir:
            raise RuntimeError("Missing shared formatting-call IR: " + required)
    run([wio, folder / "main.wio", "--emit-cpp"])
    links = ["-lws2_32", "-lshell32", "-Wa,-mbig-obj"] if os.name == "nt" else ["-pthread"]
    for optimization in ("-O0", "-O2"):
        program = folder / ("native.exe" if os.name == "nt" else "native")
        run([args.cxx, "-std=c++20", optimization, folder / "main.wio.cpp",
             f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}", args.runtime.resolve(),
             *links, "-o", program])
        run([program])
    bridge = folder / ("bridge.dll" if os.name == "nt" else "bridge.dylib" if sys.platform == "darwin" else "bridge.so")
    bytecode = folder / "main.wiob"
    run([wio, folder / "main.wio", "--target", "shared", "--native-bridge-only", "--output", bridge,
         "--bytecode-sidecar", bytecode])
    run([wio, "--vm-run-bytecode", bytecode, "--native-library", bridge])
    # The former object-array boundary must now execute shared WIR traversal.
    shutil.copyfile(HERE.parent / "formatting/custom.wio", folder / "container.wio")
    container_bridge = folder / ("container.dll" if os.name == "nt" else "container.dylib" if sys.platform == "darwin" else "container.so")
    container_bytecode = folder / "container.wiob"
    run([wio, folder / "container.wio", "--target", "shared", "--native-bridge-only", "--output", container_bridge,
         "--bytecode-sidecar", container_bytecode])
    run([wio, "--vm-run-bytecode", container_bytecode, "--native-library", container_bridge])
    shutil.copyfile(HERE / "closure.wio", folder / "closure.wio")
    run([wio, folder / "closure.wio", "--emit-cpp"])
    for optimization in ("-O0", "-O2"):
        program = folder / ("closure.exe" if os.name == "nt" else "closure")
        run([args.cxx, "-std=c++20", optimization, folder / "closure.wio.cpp",
             f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}", args.runtime.resolve(),
             *links, "-o", program])
        run([program])
    closure_bridge = folder / ("closure.dll" if os.name == "nt" else "closure.dylib" if sys.platform == "darwin" else "closure.so")
    closure_bytecode = folder / "closure.wiob"
    run([wio, folder / "closure.wio", "--target", "shared", "--native-bridge-only", "--output", closure_bridge,
         "--bytecode-sidecar", closure_bytecode])
    run([wio, "--vm-run-bytecode", closure_bytecode, "--native-library", closure_bridge])
    for source in ("native_hook.wio", "native_hook.h"):
        shutil.copyfile(HERE / source, folder / source)
    run([wio, folder / "native_hook.wio", "--emit-cpp", "--include-dir", folder])
    for optimization in ("-O0", "-O2"):
        program = folder / ("native_hook.exe" if os.name == "nt" else "native_hook")
        run([args.cxx, "-std=c++20", optimization, folder / "native_hook.wio.cpp",
             f"-I{folder}", f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}", args.runtime.resolve(),
             *links, "-o", program])
        run([program])
    hook_bridge = folder / ("hook.dll" if os.name == "nt" else "hook.dylib" if sys.platform == "darwin" else "hook.so")
    hook_bytecode = folder / "hook.wiob"
    run([wio, folder / "native_hook.wio", "--target", "shared", "--native-bridge-only", "--include-dir", folder,
         "--output", hook_bridge, "--bytecode-sidecar", hook_bytecode])
    run([wio, "--vm-run-bytecode", hook_bytecode, "--native-library", hook_bridge])
    shutil.copyfile(HERE / "borrow.wio", folder / "borrow.wio")
    shutil.copyfile(HERE / "borrow_probe.h", folder / "borrow_probe.h")
    run([wio, folder / "borrow.wio", "--emit-cpp", "--include-dir", folder])
    borrow_source = (folder / "borrow.wio.cpp").read_text(encoding="utf-8")
    for required in ('InlinePlace<std::string>::local()', '_borrow(const std::string&',
                     '_borrow(std::string&'):
        if required not in borrow_source:
            raise RuntimeError("Missing proven stack-native borrow adapter: " + required)
    for optimization in ("-O0", "-O2"):
        program = folder / ("borrow.exe" if os.name == "nt" else "borrow")
        run([args.cxx, "-std=c++20", optimization, folder / "borrow.wio.cpp",
             f"-I{folder}", f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}", args.runtime.resolve(),
             *links, "-o", program])
        run([program])
        run([program, "indirect"])
        failed = run([program, "fail"], expected=1)
        if "borrow-probe failure" not in failed.stdout + failed.stderr:
            raise RuntimeError("Native borrow exception boundary changed")
    borrow_bridge = folder / ("borrow.dll" if os.name == "nt" else "borrow.dylib" if sys.platform == "darwin" else "borrow.so")
    borrow_bytecode = folder / "borrow.wiob"
    run([wio, folder / "borrow.wio", "--target", "shared", "--native-bridge-only", "--output", borrow_bridge,
         "--include-dir", folder, "--bytecode-sidecar", borrow_bytecode])
    run([wio, "--vm-run-bytecode", borrow_bytecode, "--native-library", borrow_bridge])
    # Native function values still reach the VM's external-function rejection;
    # direct native borrows above must succeed independently of that old gap.
    indirect = run([wio, "--vm-run-bytecode", borrow_bytecode, "--native-library", borrow_bridge,
                    "--", "indirect"], expected=1)
    if "WVM1003" not in indirect.stderr:
        raise RuntimeError("Expected explicit VM indirect native-function boundary")
    failed = run([wio, "--vm-run-bytecode", borrow_bytecode, "--native-library", borrow_bridge, "--", "fail"], expected=1)
    if "borrow-probe failure" not in failed.stdout + failed.stderr:
        raise RuntimeError("VM native borrow exception boundary changed")
    shutil.copyfile(HERE / "fields.wio", folder / "fields.wio")
    run([wio, folder / "fields.wio", "--emit-cpp"])
    field_source = (folder / "fields.wio.cpp").read_text(encoding="utf-8")
    for required in ("std::int64_t* _v", "bool* _v", "double* _v", "std::addressof(",
                     "_receiver(wio::wir_backend::ObjectBorrow<", ">{_p0.Get()}); }",
                     "CheckedFormattedSize(_formatted.max_size()", "_formatted.reserve(_required)"):
        if required not in field_source:
            raise RuntimeError("Missing proven direct scalar field representation: " + required)
    # Exercise the unchanged entry via a C++ function pointer, not only through
    # the new private dispatch entry. Every proven body must still reject null
    # before touching a field. This runs before the Wio Entry in both O0/O2.
    receivers = sorted(set(re.findall(
        r"\b(_wio_f\d+)_receiver\(wio::wir_backend::ObjectBorrow<(\w+)> _p0\)", field_source)))
    if not receivers:
        raise RuntimeError("No proven receiver entries emitted")
    probes = []
    for index, (name, owner) in enumerate(receivers):
        probes.append(f"""
namespace {{ const bool receiver_probe_{index} = []() {{
    bool rejected = false;
    auto callback = &{name};
    try {{ (void)callback(wio::wir_backend::Place<wio::runtime::Ref<{owner}>>::raw(nullptr)); }}
    catch (const std::runtime_error& error) {{
        rejected = std::string_view(error.what()) == "null Wio object access";
    }}
    if (!rejected) std::abort();
    auto token = std::make_shared<wio::wir_backend::CallbackBorrowLifetime>();
    auto previous = std::exchange(wio::wir_backend::activeCallbackBorrow, token);
    auto expired = wio::wir_backend::Place<wio::runtime::Ref<{owner}>>::raw(nullptr);
    token->active.store(false);
    rejected = false;
    try {{ (void)callback(std::move(expired)); }}
    catch (const std::runtime_error& error) {{
        rejected = std::string_view(error.what()) == "expired native callback borrow";
    }}
    if (!rejected) std::abort();
    rejected = false;
    try {{ (void)wio::wir_backend::checkedObjectReceiver(static_cast<{owner}*>(nullptr)); }}
    catch (const std::runtime_error& error) {{
        rejected = std::string_view(error.what()) == "expired native callback borrow";
    }}
    if (!rejected) std::abort();
    wio::wir_backend::activeCallbackBorrow = std::move(previous);
    return true;
}}(); }}
""")
    callback_source = folder / "fields_callback.cpp"
    callback_source.write_text('#include "fields.wio.cpp"\n' + "".join(probes), encoding="utf-8")
    for optimization in ("-O0", "-O2"):
        program = folder / ("fields.exe" if os.name == "nt" else "fields")
        run([args.cxx, "-std=c++20", optimization, callback_source,
             f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}", args.runtime.resolve(),
             *links, "-o", program])
        run([program])
    field_bridge = folder / ("fields.dll" if os.name == "nt" else "fields.dylib" if sys.platform == "darwin" else "fields.so")
    field_bytecode = folder / "fields.wiob"
    run([wio, folder / "fields.wio", "--target", "shared", "--native-bridge-only", "--output", field_bridge,
         "--bytecode-sidecar", field_bytecode])
    run([wio, "--vm-run-bytecode", field_bytecode, "--native-library", field_bridge])
    shutil.copyfile(HERE / "transfers.wio", folder / "transfers.wio")
    shutil.copyfile(HERE / "transfer_probe.h", folder / "transfer_probe.h")
    run([wio, folder / "transfers.wio", "--emit-cpp", "--include-dir", folder])
    if '.initializeFrom([&]() -> std::string { return ' not in (folder / "transfers.wio.cpp").read_text(encoding="utf-8"):
        raise RuntimeError("No proven direct string initialization in the Wio loop fixture")
    for optimization in ("-O0", "-O2"):
        program = folder / ("transfers.exe" if os.name == "nt" else "transfers")
        run([args.cxx, "-std=c++20", optimization, folder / "transfers.wio.cpp",
             f"-I{folder}", f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}", args.runtime.resolve(),
             *links, "-o", program])
        run([program])
        failed = run([program, "fail"], expected=1)
        if "transfer-probe failure" not in failed.stdout + failed.stderr:
            raise RuntimeError("Forwarded native string result changed exception propagation")
        failed = run([program, "init-fail"], expected=1)
        if "transfer-probe failure" not in failed.stdout + failed.stderr:
            raise RuntimeError("Direct native string initialization changed exception propagation")
    transfer_bridge = folder / ("transfers.dll" if os.name == "nt" else "transfers.dylib" if sys.platform == "darwin" else "transfers.so")
    transfer_bytecode = folder / "transfers.wiob"
    run([wio, folder / "transfers.wio", "--target", "shared", "--native-bridge-only", "--include-dir", folder,
         "--output", transfer_bridge, "--bytecode-sidecar", transfer_bytecode])
    run([wio, "--vm-run-bytecode", transfer_bytecode, "--native-library", transfer_bridge])
    failed = run([wio, "--vm-run-bytecode", transfer_bytecode, "--native-library", transfer_bridge,
                  "--", "fail"], expected=1)
    if "transfer-probe failure" not in failed.stdout + failed.stderr:
        raise RuntimeError("VM native string result changed exception propagation")
    failed = run([wio, "--vm-run-bytecode", transfer_bytecode, "--native-library", transfer_bridge,
                  "--", "init-fail"], expected=1)
    if "transfer-probe failure" not in failed.stdout + failed.stderr:
        raise RuntimeError("VM direct string initialization changed exception propagation")
    shutil.copyfile(HERE / "fragments.wio", folder / "fragments.wio")
    shutil.copyfile(HERE / "fragment_probe.h", folder / "fragment_probe.h")
    run([wio, folder / "fragments.wio", "--emit-cpp", "--include-dir", folder])
    fragment_source = (folder / "fragments.wio.cpp").read_text(encoding="utf-8")
    for required in ("wio::runtime::detail::RenderedInteger _v", ".format(", ".view())", " = std::string_view{"):
        if required not in fragment_source:
            raise RuntimeError("Missing rendered scalar fragment representation: " + required)
    for optimization in ("-O0", "-O2"):
        program = folder / ("fragments.exe" if os.name == "nt" else "fragments")
        run([args.cxx, "-std=c++20", optimization, folder / "fragments.wio.cpp",
             f"-I{folder}", f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}", args.runtime.resolve(),
             *links, "-o", program])
        run([program])
    fragment_bridge = folder / ("fragments.dll" if os.name == "nt" else "fragments.dylib" if sys.platform == "darwin" else "fragments.so")
    fragment_bytecode = folder / "fragments.wiob"
    run([wio, folder / "fragments.wio", "--target", "shared", "--native-bridge-only", "--include-dir", folder,
         "--output", fragment_bridge, "--bytecode-sidecar", fragment_bytecode])
    # MSVC host / GCC bridge may not share std::locale::global. Runtime
    # custom-facet tests and native O0/O2 cover that representation contract;
    # VM checks the unchanged NUL/Text/bool/integer mixed-hook surface.
    run([wio, "--vm-run-bytecode", fragment_bytecode, "--native-library", fragment_bridge, "--", "vm"])
    (folder / "results.json").write_text(json.dumps(dict(passed=True, processes=rows), indent=2) + "\n")
    print("N03 scalar ToString: ordered/null-safe dispatch, view/interface/generic calls, native O0/O2 + VM")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wio", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--output", type=Path, required=True)
    verify(parser.parse_args())
