from dotenv import load_dotenv
load_dotenv()

from fastapi import FastAPI, File, UploadFile, Form, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
import shutil
from pathlib import Path
import os

from src.pipeline import InferencePipeline

app = FastAPI(title="TalentMatch AI API", description="API for scoring resumes against job descriptions")

# Configure CORS for frontend access
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # In production, specify the domain
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

pipeline = InferencePipeline()

UPLOAD_DIR = Path("uploads")
UPLOAD_DIR.mkdir(exist_ok=True)

@app.post("/api/score")
async def score_resume(
    pdf_file: UploadFile = File(None),
    job_description: str = Form(...),
    resume_text: str = Form(None)
):
    if not job_description or not job_description.strip():
        raise HTTPException(status_code=400, detail="Job description cannot be empty.")

    if not pdf_file and not resume_text:
        raise HTTPException(status_code=400, detail="Must provide either a pdf_file or resume_text.")

    temp_path = None
    try:
        if pdf_file:
            if not pdf_file.filename.endswith(".pdf"):
                raise HTTPException(status_code=400, detail="Only PDF files are supported.")
            
            temp_path = UPLOAD_DIR / pdf_file.filename
            with temp_path.open("wb") as buffer:
                shutil.copyfileobj(pdf_file.file, buffer)
            result = pipeline.run(resume_pdf_path=temp_path, job_description_text=job_description)
        else:
            result = pipeline.run(resume_text=resume_text, job_description_text=job_description)
        
        return JSONResponse(content={
            "score": result["score"],
            "raw_similarity": result["raw_similarity"],
            "explanation": result["explanation"]
        })
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
    finally:
        # Cleanup
        if temp_path and temp_path.exists():
            os.remove(temp_path)

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
