"""
api.py  —  TalentMatch v2 FastAPI endpoint.

Python responsibilities here:
  - Receive multipart/form-data request
  - Validate inputs
  - Save/cleanup temporary PDF
  - Call InferencePipeline (which calls C++ engine)
  - Return JSON response

Python does NOT compute any scores, rank, or explain anything.
"""

from dotenv import load_dotenv
load_dotenv()

from typing import Optional
from fastapi import FastAPI, File, UploadFile, Form, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse, FileResponse
from fastapi.staticfiles import StaticFiles
import shutil
from pathlib import Path
import os

from src.pipeline import InferencePipeline

app = FastAPI(
    title="TalentMatch AI v2",
    description=(
        "Resume Ranking Engine — deterministic ML scoring via a native C++ engine. "
        "No LLM, no Groq, no hardcoded formulas."
    ),
    version="2.0.0",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

pipeline = InferencePipeline()

@app.on_event("startup")
async def startup_event():
    from src.embeddings.generator import _EmbeddingModelProvider
    import logging
    logger = logging.getLogger("api")
    logger.info("Pre-warming embedding model on startup...")
    _EmbeddingModelProvider.get()
    logger.info("Embedding model pre-warmed and ready.")

UPLOAD_DIR = Path("uploads")
UPLOAD_DIR.mkdir(exist_ok=True)


@app.post("/api/score")
async def score_resume(
    pdf_file: Optional[UploadFile] = File(None),
    job_description: str = Form(...),
    resume_text: Optional[str] = Form(None),
):
    """
    Score a resume against a job description.

    Request: multipart/form-data
      - pdf_file (file, optional)    : PDF resume upload
      - job_description (str)        : Job description text
      - resume_text (str, optional)  : Plain text resume (alternative to PDF)

    Response: application/json
    {
      "overall_score":        float,   // 0–100
      "scores": {
        "skills":             float,
        "experience":         float,
        "education":          float,
        "projects":           float,
        "semantic":           float,
        "resume_quality":     float
      },
      "matched_skills":       [str],
      "missing_skills":       [str],
      "partial_skills":       [{"jd_skill": str, "matched_as": str, "confidence": float}],
      "top_positive_factors": [str],
      "top_negative_factors": [str],
      "ranking_method":       str      // "xgboost" | "linear_fallback"
    }
    """
    if not job_description or not job_description.strip():
        raise HTTPException(status_code=400, detail="job_description cannot be empty.")

    if not pdf_file and not resume_text:
        raise HTTPException(status_code=400, detail="Must provide either pdf_file or resume_text.")

    temp_path = None
    try:
        if pdf_file:
            if not pdf_file.filename.lower().endswith(".pdf"):
                raise HTTPException(status_code=400, detail="Only PDF files are supported.")
            temp_path = UPLOAD_DIR / pdf_file.filename
            with temp_path.open("wb") as buffer:
                shutil.copyfileobj(pdf_file.file, buffer)
            result = pipeline.run(
                resume_pdf_path=temp_path,
                job_description_text=job_description,
            )
        else:
            result = pipeline.run(
                resume_text=resume_text,
                job_description_text=job_description,
            )

        # Return the full structured response
        return JSONResponse(content={
            "overall_score":        result.get("overall_score", 0.0),
            "scores":               result.get("scores", {}),
            "matched_skills":       result.get("matched_skills", []),
            "missing_skills":       result.get("missing_skills", []),
            "partial_skills":       result.get("partial_skills", []),
            "top_positive_factors": result.get("top_positive_factors", []),
            "top_negative_factors": result.get("top_negative_factors", []),
            "ranking_method":       result.get("ranking_method", "unknown"),
            "feature_vector":       result.get("feature_vector", {}),
        })

    except (ValueError, RuntimeError) as e:
        # Treat PDF parsing failures or invalid inputs as a Bad Request
        raise HTTPException(status_code=400, detail=str(e))
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
    finally:
        if temp_path and temp_path.exists():
            os.remove(temp_path)


@app.get("/api/health")
async def health():
    """Health check and engine version."""
    try:
        from src.bridge import engine_version
        version = engine_version()
    except Exception as e:
        version = f"unavailable ({e})"
    return {"status": "ok", "engine_version": version, "api_version": "2.0.0"}

# ── Serve built Vite frontend ─────────────────────────────────────────────────
# Docker builds frontend/dist via the Node.js stage.
# Falls back to raw frontend/ folder for local dev without a build.

FRONTEND_DIST = Path("frontend/dist")
FRONTEND_RAW  = Path("frontend")

if FRONTEND_DIST.exists():
    _serve_dir = FRONTEND_DIST
elif FRONTEND_RAW.exists():
    _serve_dir = FRONTEND_RAW
else:
    _serve_dir = None

if _serve_dir:
    # Mount static assets subfolder if it exists
    _assets = _serve_dir / "assets"
    if _assets.exists():
        app.mount("/assets", StaticFiles(directory=str(_assets)), name="assets")

    @app.get("/")
    async def serve_index():
        return FileResponse(_serve_dir / "index.html")

    @app.get("/{full_path:path}")
    async def serve_spa(full_path: str):
        file_path = _serve_dir / full_path
        if file_path.is_file():
            return FileResponse(str(file_path))
        return FileResponse(_serve_dir / "index.html")

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
