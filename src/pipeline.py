"""
src/pipeline.py  —  TalentMatch v2 inference pipeline.

Python responsibilities (only):
  1. Extract plain text from PDF (PyMuPDF)
  2. Generate embeddings (sentence-transformers, local, no network)
  3. Call C++ engine via ctypes bridge
  4. Return JSON result

Python does NOT:
  - parse the resume structure
  - extract or normalize skills
  - compute any ATS score or feature
  - rank or explain
"""

from __future__ import annotations

from pathlib import Path
from typing import Optional

from src.bridge import score as cpp_score
from src.config import get_logger
from src.embeddings.generator import embed

logger = get_logger("pipeline")


class InferencePipeline:
    """
    Single-resume, single-JD orchestration.

    Usage:
        pipeline = InferencePipeline()
        result = pipeline.run(resume_pdf_path=Path("resume.pdf"),
                              job_description_text="Senior backend engineer...")
    """

    def run(
        self,
        resume_pdf_path: Optional[Path] = None,
        job_description_text: str = "",
        resume_text: Optional[str] = None,
    ) -> dict:
        """
        Args:
            resume_pdf_path:       Path to resume PDF (mutually exclusive with resume_text).
            job_description_text:  Raw job description text.
            resume_text:           Plain text resume (mutually exclusive with resume_pdf_path).

        Returns:
            {
              "overall_score":        float,   # 0–100
              "scores":               dict,    # per-category 0–100
              "matched_skills":       list,
              "missing_skills":       list,
              "partial_skills":       list,
              "top_positive_factors": list,
              "top_negative_factors": list,
              "feature_vector":       dict,    # all ~85 features (for debug)
              "ranking_method":       str,     # "xgboost" or "linear_fallback"
            }
        """
        if not job_description_text or not job_description_text.strip():
            raise ValueError("job_description_text cannot be empty")

        # --- Step 1: Get resume plain text ---
        if resume_text:
            raw_text = resume_text
        elif resume_pdf_path:
            raw_text = self._extract_pdf_text(Path(resume_pdf_path))
        else:
            raise ValueError("Either resume_pdf_path or resume_text must be provided")

        logger.info(f"Resume text extracted: {len(raw_text)} chars")

        # --- Step 2: Generate embeddings (Python, local, sentence-transformers) ---
        logger.info("Generating embeddings...")
        resume_emb = embed(raw_text).tolist()
        jd_emb     = embed(job_description_text).tolist()
        logger.info(f"Embeddings ready: dim={len(resume_emb)}")

        # --- Step 3: Call C++ engine ---
        logger.info("Calling C++ engine...")
        result = cpp_score(
            resume_text=raw_text,
            jd_text=job_description_text,
            resume_embedding=resume_emb,
            jd_embedding=jd_emb,
        )

        logger.info(
            f"Score: {result.get('overall_score')} | "
            f"Method: {result.get('ranking_method')} | "
            f"Matched skills: {len(result.get('matched_skills', []))}"
        )

        return result

    @staticmethod
    def _extract_pdf_text(pdf_path: Path) -> str:
        """Extract plain text from a PDF using PyMuPDF."""
        try:
            import fitz  # PyMuPDF

            doc = fitz.open(str(pdf_path))
            pages = []
            for page in doc:
                text = page.get_text("text").strip()
                if text:
                    pages.append(text)
            doc.close()
            full_text = "\n\n".join(pages)

            if not full_text.strip():
                logger.warning(f"No native text in {pdf_path.name} — trying OCR block extraction")
                # Fallback: try get_text("blocks") which sometimes recovers more
                doc2 = fitz.open(str(pdf_path))
                blocks = []
                for page in doc2:
                    for block in page.get_text("blocks"):
                        if block[6] == 0:  # text block
                            blocks.append(block[4])
                doc2.close()
                full_text = "\n".join(blocks)

            return full_text.strip()

        except Exception as exc:
            raise RuntimeError(f"Failed to extract text from '{pdf_path}': {exc}") from exc
