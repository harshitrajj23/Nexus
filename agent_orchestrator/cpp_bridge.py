#!/usr/bin/env python3
"""
NEXUS C++ Engine Bridge Client
Manages a persistent subprocess connection to the C++17 LibTorch nexus_engine_bridge,
communicating over high-throughput JSONL stdio pipes for sub-millisecond inference,
graph state updates, and multi-hop causal attack path reconstruction.
"""

import os
import json
import logging
import subprocess
import threading
from pathlib import Path
from typing import Dict, Any, Optional

logger = logging.getLogger("nexus.cpp_bridge")


class NexusCppBridge:
    """
    Client connecting Python orchestration to the native C++17 LibTorch engine.
    Maintains a dedicated subprocess running nexus_engine_bridge.
    """

    def __init__(
        self,
        binary_path: Optional[str] = None,
        model_path: Optional[str] = None
    ):
        base_dir = Path(__file__).resolve().parent.parent
        self.binary_path = Path(binary_path) if binary_path else base_dir / "cpp_engine" / "build" / "nexus_engine_bridge"
        self.model_path = Path(model_path) if model_path else base_dir / "data" / "nexus_best.pt"

        if not self.binary_path.exists():
            raise FileNotFoundError(
                f"nexus_engine_bridge binary not found at '{self.binary_path}'. "
                "Build it first with: cmake --build cpp_engine/build --target nexus_engine_bridge"
            )

        self._process: Optional[subprocess.Popen] = None
        self._lock = threading.Lock()
        self._start_engine()

    def _start_engine(self) -> None:
        """Launches the C++ engine bridge subprocess."""
        cmd = [str(self.binary_path), "--model", str(self.model_path)]
        logger.info(f"Starting C++ engine bridge: {' '.join(cmd)}")
        self._process = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1
        )

    def is_alive(self) -> bool:
        """Checks if the C++ subprocess is running."""
        return self._process is not None and self._process.poll() is None

    def _ensure_alive(self) -> None:
        """Restarts the process if it exited unexpectedly."""
        if not self.is_alive():
            logger.warning("nexus_engine_bridge process terminated. Restarting...")
            self._start_engine()

    def send_command(self, cmd_dict: Dict[str, Any]) -> Dict[str, Any]:
        """Sends a JSON command to the C++ engine and reads the response."""
        with self._lock:
            self._ensure_alive()
            assert self._process is not None
            assert self._process.stdin is not None
            assert self._process.stdout is not None

            line = json.dumps(cmd_dict) + "\n"
            try:
                self._process.stdin.write(line)
                self._process.stdin.flush()
                resp_line = self._process.stdout.readline()
                if not resp_line:
                    raise RuntimeError("Received empty response from nexus_engine_bridge")
                return json.loads(resp_line.strip())
            except Exception as e:
                logger.error(f"Error communicating with C++ engine: {e}")
                self._start_engine()
                raise

    def health(self) -> Dict[str, Any]:
        """Checks C++ engine bridge status and loaded model."""
        return self.send_command({"cmd": "health"})

    def reset(self) -> Dict[str, Any]:
        """Resets the dynamic graph and temporal sequence state in the C++ engine."""
        return self.send_command({"cmd": "reset"})

    def snapshot(self) -> Dict[str, Any]:
        """Retrieves graph nodes, edges, and active attack paths for visualization."""
        return self.send_command({"cmd": "snapshot"})

    def ingest_event(self, event: Dict[str, Any]) -> Dict[str, Any]:
        """
        Ingests a TelemetryEvent into the dynamic graph, runs LibTorch TemporalRiskModel
        inference, reconstructs attack paths, and returns computed prediction and metrics.
        """
        return self.send_command({
            "cmd": "ingest",
            "event": event
        })

    def close(self) -> None:
        """Terminates the C++ bridge subprocess."""
        with self._lock:
            if self._process and self._process.poll() is None:
                self._process.terminate()
                try:
                    self._process.wait(timeout=2.0)
                except subprocess.TimeoutExpired:
                    self._process.kill()
            self._process = None
