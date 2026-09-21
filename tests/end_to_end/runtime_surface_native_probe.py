#!/usr/bin/env python3
"""Prove process-scoped runtime queries using owned native children and loopback sockets.

Run under the station lease and qualification guard on a disposable proof tree.
This observes the inspector's native contracts, not BPR package qualification.
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import platform
import socket
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import verify_runtime_surface as surface


def main() -> int:
    if "QUALIFICATION_TARGET_ROOT" not in os.environ:
        raise RuntimeError("qualification guard required")
    child = subprocess.Popen([sys.executable, "-I", "-S", "-c", "import time; time.sleep(45)"])
    try:
        children = surface.child_processes(os.getpid())
        if child.pid not in children:
            raise RuntimeError("actual direct child was not observed")
        with socket.socket() as listener, socket.socket() as client:
            listener.bind(("127.0.0.1", 0))
            listener.listen(1)
            client.settimeout(5)
            client.connect(listener.getsockname())
            connection, _ = listener.accept()
            with connection:
                sockets = surface.network_sockets(os.getpid())
                for port in (listener.getsockname()[1], client.getsockname()[1]):
                    spelling = f":{port:04X}" if platform.system() == "Linux" else f":{port}"
                    if not any(spelling in row for row in sockets):
                        raise RuntimeError("actual owned listener or connected socket was not observed")
                if surface.network_sockets(child.pid):
                    raise RuntimeError("socket query included another process's sockets")
        modules = surface.loaded_libraries(os.getpid())
        if not modules:
            raise RuntimeError("actual native library inspection returned no evidence")
        unix_observed = "not-implemented-on-Windows"
        if platform.system() != "Windows":
            with tempfile.TemporaryDirectory(dir=os.environ["QUALIFICATION_TARGET_ROOT"], prefix="ipc-") as directory:
                endpoint = Path(directory) / "p.sock"
                with socket.socket(socket.AF_UNIX) as listener:
                    listener.bind(str(endpoint))
                    endpoint.chmod(0o600)
                    listener.listen(1)
                    surface.check_profile_socket(os.getpid(), endpoint)
                    if surface.named_unix_sockets(child.pid):
                        raise RuntimeError("Unix socket query included another process's socket")
                    endpoint.chmod(0o666)
                    try:
                        surface.check_profile_socket(os.getpid(), endpoint)
                    except RuntimeError:
                        pass
                    else:
                        raise RuntimeError("unsafe Unix socket mode was accepted")
                unix_observed = "owned endpoint observed; unrelated process excluded; unsafe mode rejected"
    finally:
        child.terminate()
        try:
            child.wait(timeout=5)
        except subprocess.TimeoutExpired:
            child.kill()
            child.wait(timeout=5)
    print(json.dumps({"result": "NATIVE_RUNTIME_QUERY_OK", "platform": platform.system(),
                      "architecture": platform.machine(), "python": platform.python_version(),
                      "direct_child_observed": True, "owned_listener_and_connection_observed": True,
                      "other_process_sockets_excluded": True, "loaded_library_count": len(modules),
                      "unix_socket": unix_observed,
                      "child_reaped": child.poll() is not None, "package_qualification": "not-claimed"}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
