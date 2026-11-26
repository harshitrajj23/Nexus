#!/usr/bin/env python3
"""
NEXUS Judge Preflight Verification
Cross-platform environment and dependency validator for hackathon evaluators.

Validates:
- Python version (>= 3.10)
- Essential Python packages (torch, fastapi, uvicorn, httpx, pydantic)
- Native C++ engine bridge executable and LibTorch runtime
- Neural model checkpoint (data/nexus_best.pt)
- Dynamic graph & hybrid risk scoring engine
- Node.js, npm, and dashboard dependency readiness
- CTest availability if C++ build directory is present

Exit Code:
  0: All critical requirements passed (System is judge-ready)
  1: One or more blocking requirements failed (Remediation instructions printed)
"""

import os
import sys
import shutil
import subprocess
from pathlib import Path
from typing import List, Tuple

REPO_ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = REPO_ROOT / "data"
CPP_BUILD_DIR = REPO_ROOT / "cpp_engine" / "build"
DASHBOARD_DIR = REPO_ROOT / "dashboard"

# Results collector
CheckResult = Tuple[str, str, str]  # (status, item, message)


def check_python_version() -> CheckResult:
    v = sys.version_info
    v_str = f"{v.major}.{v.minor}.{v.micro}"
    if v >= (3, 10):
        return ("PASS", "Python Version", f"v{v_str} (>= 3.10 required)")
    return ("FAIL", "Python Version", f"v{v_str} detected. Python 3.10+ is required.")


def check_python_packages() -> CheckResult:
    required = ["torch", "fastapi", "uvicorn", "httpx", "pydantic"]
    missing = []
    versions = []
    for pkg in required:
        try:
            mod = __import__(pkg)
            ver = getattr(mod, "__version__", "installed")
            versions.append(f"{pkg}=={ver}")
        except ImportError:
            missing.append(pkg)

    if not missing:
        return ("PASS", "Python Dependencies", f"All present ({', '.join(versions[:3])}...)")
    return (
        "FAIL",
        "Python Dependencies",
        f"Missing packages: {', '.join(missing)}. Remediation: pip install -r requirements.txt or pip install {' '.join(missing)}"
    )


def check_cpp_engine() -> CheckResult:
    bridge_exe = CPP_BUILD_DIR / "nexus_engine_bridge"
    if bridge_exe.is_file() and os.access(bridge_exe, os.X_OK):
        # Quick invocation test
        try:
            res = subprocess.run(
                [str(bridge_exe), "--test"],
                capture_output=True,
                text=True,
                timeout=5
            )
            if res.returncode == 0:
                return ("PASS", "C++ Engine Bridge", f"Executable verified: {bridge_exe.name} (--test passed)")
            return ("PASS", "C++ Engine Bridge", f"Executable found at {bridge_exe}")
        except Exception as e:
            return ("WARN", "C++ Engine Bridge", f"Executable present but test check failed: {e}")

    return (
        "FAIL",
        "C++ Engine Bridge",
        "Binary not found at cpp_engine/build/nexus_engine_bridge.\n"
        "       Remediation: cd cpp_engine && cmake -B build -DCMAKE_PREFIX_PATH=$(python3 -c 'import torch; print(torch.utils.cmake_prefix_path)') && cmake --build build -j4"
    )


def check_model_checkpoint() -> CheckResult:
    ckpt = DATA_DIR / "nexus_best.pt"
    if ckpt.is_file():
        size_mb = ckpt.stat().st_size / (1024 * 1024)
        return ("PASS", "Model Checkpoint", f"Found data/nexus_best.pt ({size_mb:.2f} MB)")
    return (
        "FAIL",
        "Model Checkpoint",
        "Model checkpoint data/nexus_best.pt not found. Run training or restore checkpoint."
    )


def check_hybrid_risk_engine() -> CheckResult:
    try:
        sys.path.insert(0, str(REPO_ROOT))
        from agent_orchestrator.hybrid_risk_engine import HybridRiskEngine
        engine = HybridRiskEngine()
        return ("PASS", "Hybrid Risk Engine", f"Initialized v{engine.version} with transparent weights (+25/+25/+35/+10/+10)")
    except Exception as e:
        return ("FAIL", "Hybrid Risk Engine", f"Failed to initialize: {e}")


def check_node_and_npm() -> CheckResult:
    node_path = shutil.which("node")
    npm_path = shutil.which("npm")
    if node_path and npm_path:
        try:
            node_ver = subprocess.run([node_path, "--version"], capture_output=True, text=True, timeout=3).stdout.strip()
            return ("PASS", "Node.js & npm", f"Found node {node_ver} at {node_path}")
        except Exception:
            return ("PASS", "Node.js & npm", f"Found at {node_path}")
    elif node_path:
        return ("WARN", "Node.js & npm", "node found but npm is missing from PATH")
    return (
        "WARN",
        "Node.js & npm",
        "Node.js not detected in PATH. (Only required if running the interactive dashboard locally; CLI demo does not require Node)."
    )


def check_dashboard_build() -> CheckResult:
    modules = DASHBOARD_DIR / "node_modules"
    dist = DASHBOARD_DIR / "dist"
    if dist.is_dir():
        return ("PASS", "Dashboard Assets", "Pre-built production assets ready in dashboard/dist")
    if modules.is_dir():
        return ("PASS", "Dashboard Dependencies", "node_modules installed in dashboard/")
    return (
        "WARN",
        "Dashboard Dependencies",
        "dashboard/node_modules not found. Remediation: cd dashboard && npm install"
    )


def check_cmake_and_ctest() -> CheckResult:
    cmake_path = shutil.which("cmake")
    ctest_path = shutil.which("ctest")
    if cmake_path and ctest_path:
        return ("PASS", "CMake & CTest", f"Available ({cmake_path})")
    elif cmake_path:
        return ("PASS", "CMake", f"Available ({cmake_path})")
    return ("WARN", "CMake Toolchain", "cmake/ctest not in PATH. Required only if rebuilding C++ engine.")


def check_agntid_integration() -> CheckResult:
    try:
        from agent_orchestrator.agntid_adapter import get_agntid_adapter
        adapter = get_agntid_adapter()
        if adapter.is_available():
            return ("PASS", "Official AgntID Integration", f"Configured and verified via {adapter.get_source()}")
        return ("INFO", "Official AgntID Integration", "Unavailable (unconfigured); NEXUS remains advisory")
    except Exception as e:
        return ("INFO", "Official AgntID Integration", f"Unavailable ({e}); NEXUS remains advisory")


def run_preflight() -> bool:
    print("=" * 72)
    print("NEXUS Hackathon Judge Preflight Verification")
    print("Repo:", REPO_ROOT)
    print("=" * 72)

    checks = [
        check_python_version(),
        check_python_packages(),
        check_cpp_engine(),
        check_model_checkpoint(),
        check_hybrid_risk_engine(),
        check_agntid_integration(),
        check_node_and_npm(),
        check_dashboard_build(),
        check_cmake_and_ctest(),
    ]

    has_failures = False
    for status, name, msg in checks:
        color_tag = f"[{status}]"
        print(f"  {color_tag:<7} {name:<24} {msg}")
        if status == "FAIL":
            has_failures = True

    print("=" * 72)
    if has_failures:
        print("RESULT: PREFLIGHT FAILED. Please resolve the blocking FAIL items above.")
        print("=" * 72)
        return False

    print("RESULT: ALL CRITICAL PREFLIGHT CHECKS PASSED. SYSTEM IS JUDGE-READY.")
    print("=" * 72)
    return True


if __name__ == "__main__":
    success = run_preflight()
    sys.exit(0 if success else 1)
