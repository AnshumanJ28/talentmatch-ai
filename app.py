import gradio as gr

from pathlib import Path

from src.pipeline import InferencePipeline

pipeline = InferencePipeline()


def score_resume(pdf_file, job_description):
    if pdf_file is None or not job_description.strip():
        return None, {"error": "Please upload a PDF and paste a job description."}
    result = pipeline.run(Path(pdf_file.name), job_description)
    return result["score"], result["explanation"]


demo = gr.Interface(
    fn=score_resume,
    inputs=[
        gr.File(label="Upload Resume (PDF)", file_types=[".pdf"]),
        gr.Textbox(label="Job Description", lines=10, placeholder="Paste the JD here..."),
    ],
    outputs=[
        gr.Number(label="ATS Match Score"),
        gr.JSON(label="Explanation / Breakdown"),
    ],
    title="TalentMatch AI — Resume ATS Scorer",
    description="Upload a resume PDF and a job description to get a match score with explanation.",
)

if __name__ == "__main__":
    demo.launch(
    server_name="0.0.0.0",
    server_port=int(os.environ.get("PORT", 7860))
)
