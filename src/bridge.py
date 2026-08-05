"""
src/bridge.py  —  ctypes FFI bridge to the TalentMatch C++ shared library.

Python is the ONLY caller of this module.
The C++ library is called via ctypes with a JSON string interface.

Interface contract (matches cpp_core/include/engine.h):
    engine_score(request_json: bytes) -> bytes   # heap-allocated, call engine_free()
    engine_free(ptr: c_char_p)                   # releases heap-allocated string
    engine_version() -> bytes                    # static string, do NOT free

Memory safety:
    engine_score returns a heap-allocated C string.
    This module calls engine_free() immediately after copying the bytes into Python.
    Python never holds a raw C pointer beyond the bridge call.
"""

from __future__ import annotations

import ctypes
import json
import os
import platform
import sys
from pathlib import Path
from typing import Optional

from src.config import get_logger

logger = get_logger("bridge")


# ---------------------------------------------------------------------------
# Library discovery
# ---------------------------------------------------------------------------

def _find_library() -> Optional[Path]:
    """
    Searches for the compiled shared library in order of precedence:
      1. TALENTMATCH_LIB_PATH environment variable (explicit override)
      2. cpp_core/build/ relative to the project root
      3. The directory containing this file (for containerized installs)
    """
    # Env override
    env = os.environ.get("TALENTMATCH_LIB_PATH")
    if env:
        p = Path(env)
        if p.exists():
            return p
        logger.warning(f"TALENTMATCH_LIB_PATH set to '{env}' but file not found.")

    # Platform-specific library name
    system = platform.system()
    if system == "Windows":
        name = "talentmatch.dll"
    elif system == "Darwin":
        name = "talentmatch.dylib"
    else:
        name = "talentmatch.so"

    # Look in cpp_core/build/ starting from project root
    here = Path(__file__).resolve()
    for root_candidate in [here.parent.parent, here.parent]:
        candidates = [
            root_candidate / "cpp_core" / "build" / name,
            root_candidate / "cpp_core" / "build" / "Release" / name,  # MSVC
            root_candidate / "cpp_core" / "build" / "Debug" / name,
            root_candidate / name,  # local install
        ]
        for p in candidates:
            if p.exists():
                return p

    return None


# ---------------------------------------------------------------------------
# Library loader
# ---------------------------------------------------------------------------

class _TalentMatchLib:
    """
    Wraps the ctypes handle and exposes the three C-linkage functions.
    Loaded once as a module-level singleton.
    """

    def __init__(self, lib_path: Path) -> None:
        self._lib = ctypes.CDLL(str(lib_path))
        self._configure_signatures()
        logger.info(f"TalentMatch C++ engine loaded: {lib_path}")
        logger.info(f"Engine version: {self.version()}")

    def _configure_signatures(self) -> None:
        # const char* engine_score(const char*)
        self._lib.engine_score.argtypes  = [ctypes.c_char_p]
        self._lib.engine_score.restype   = ctypes.c_void_p

        # void engine_free(const char*)
        self._lib.engine_free.argtypes   = [ctypes.c_void_p]
        self._lib.engine_free.restype    = None

        # const char* engine_version()
        self._lib.engine_version.argtypes = []
        self._lib.engine_version.restype  = ctypes.c_char_p

    def score(self, request: dict) -> dict:
        """
        Main call: serialize request dict to JSON, call engine_score(),
        free the C buffer, return the parsed response dict.
        """
        request_bytes = json.dumps(request).encode("utf-8")
        raw_ptr = self._lib.engine_score(request_bytes)
        if not raw_ptr:
            raise RuntimeError("engine_score returned NULL — fatal C++ error")
        try:
            response_bytes = ctypes.string_at(raw_ptr)
            response_str = response_bytes.decode("utf-8")
        finally:
            # Always free even if decode fails
            self._lib.engine_free(raw_ptr)

        result = json.loads(response_str)
        if result.get("error"):
            raise RuntimeError(f"C++ engine error: {result['error']}")
        return result

    def version(self) -> str:
        raw = self._lib.engine_version()
        return raw.decode("utf-8") if raw else "unknown"


# ---------------------------------------------------------------------------
# Module-level singleton (lazy load on first use)
# ---------------------------------------------------------------------------

_lib_instance: Optional[_TalentMatchLib] = None
_lib_unavailable: bool = False


def _get_lib() -> _TalentMatchLib:
    global _lib_instance, _lib_unavailable

    if _lib_instance is not None:
        return _lib_instance

    if _lib_unavailable:
        raise RuntimeError(
            "TalentMatch C++ engine library is not available. "
            "Run: cd cpp_core && mkdir build && cd build && cmake .. && cmake --build . --config Release"
        )

    lib_path = _find_library()
    if lib_path is None:
        _lib_unavailable = True
        raise RuntimeError(
            "TalentMatch C++ library not found. Build it first:\n"
            "  cd cpp_core && mkdir build && cd build && cmake .. && cmake --build . --config Release\n"
            "Then set TALENTMATCH_LIB_PATH if placing the library in a custom location."
        )

    try:
        _lib_instance = _TalentMatchLib(lib_path)
    except OSError as exc:
        _lib_unavailable = True
        raise RuntimeError(f"Failed to load TalentMatch library from {lib_path}: {exc}") from exc

    return _lib_instance


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def score(
    resume_text: str,
    jd_text: str,
    resume_embedding: list[float],
    jd_embedding: list[float],
    taxonomy_path: str = "data/configs/skill_taxonomy.json",
    model_path: str = "models/xgboost_model.json",
) -> dict:
    """
    Call the C++ engine with the resume and JD texts + pre-computed embeddings.

    Args:
        resume_text:       Plain text extracted from the resume PDF.
        jd_text:           Job description raw text.
        resume_embedding:  384-dim float list from sentence-transformers.
        jd_embedding:      384-dim float list from sentence-transformers.
        taxonomy_path:     Optional override for skill taxonomy JSON path.
        model_path:        Optional override for XGBoost model path.

    Returns:
        {
          "overall_score":        float,  # 0–100
          "scores":               {...},
          "matched_skills":       [...],
          "missing_skills":       [...],
          "partial_skills":       [...],
          "top_positive_factors": [...],
          "top_negative_factors": [...],
          "feature_vector":       {...},
          "ranking_method":       str
        }

    Raises:
        RuntimeError: if the C++ library is not built or the engine returns an error.
    """
    lib = _get_lib()
    request = {
        "resume_text":       resume_text,
        "jd_text":           jd_text,
        "resume_embedding":  resume_embedding,
        "jd_embedding":      jd_embedding,
        "taxonomy_path":     taxonomy_path,
        "model_path":        model_path,
    }
    return lib.score(request)


def engine_version() -> str:
    """Returns the C++ engine semantic version string."""
    return _get_lib().version()
