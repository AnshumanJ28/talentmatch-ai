"""
app.py  —  TalentMatch v2 Gradio UI shim.

Kept for backward compatibility and local dev convenience.
The main API is api.py (FastAPI on port 8000).
"""

import gradio as gr
from pathlib import Path

from src.pipeline import InferencePipeline

pipeline = InferencePipeline()


def score_resume(pdf_file, job_description):
    if pdf_file is None or not job_description.strip():
        return None, {}, [], [], [], []
    try:
        result = pipeline.run(Path(pdf_file.name), job_description)
        return (
            result.get("overall_score", 0.0),
            result.get("scores", {}),
            result.get("matched_skills", []),
            result.get("missing_skills", []),
            result.get("top_positive_factors", []),
            result.get("top_negative_factors", []),
        )
    except Exception as e:
        return None, {"error": str(e)}, [], [], [], []


demo = gr.Interface(
    fn=score_resume,
    inputs=[
        gr.File(label="Upload Resume (PDF)", file_types=[".pdf"]),
        gr.Textbox(label="Job Description", lines=10, placeholder="Paste the JD here..."),
    ],
    outputs=[
        gr.Number(label="Overall ATS Score (0–100)"),
        gr.JSON(label="Score Breakdown"),
        gr.JSON(label="Matched Skills"),
        gr.JSON(label="Missing Skills"),
        gr.JSON(label="Top Positive Factors"),
        gr.JSON(label="Top Negative Factors"),
    ],
    title="TalentMatch AI v2 — Resume Ranking Engine",
    description=(
        "Upload a resume PDF and paste a job description. "
        "Scoring is powered by a native C++ ML engine — no LLM, no API key required."
    ),
)

if __name__ == "__main__":
    demo.launch(server_name="0.0.0.0", server_port=7860)
