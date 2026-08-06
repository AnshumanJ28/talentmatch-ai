import time
import psutil
import os
import gc
from pathlib import Path
import fitz

from src.pipeline import InferencePipeline

SAMPLE_RESUME_TEXT = """Jordan Ellis
Email: jordan.ellis@example.com | Phone: 555-201-4477
Location: Austin, TX | LinkedIn: linkedin.com/in/jordanellis

SUMMARY
Backend engineer with 6 years building distributed systems in fintech.

EXPERIENCE
Senior Backend Engineer, Northwind Payments, Austin, TX
Jan 2022 - Present
- Led migration of the ledger service from monolith to microservices
- Reduced p99 latency by 40 percent using Redis caching layer

Backend Engineer, Fintech Labs, Austin, TX
Jun 2019 - Dec 2021
- Built the fraud detection pipeline using Kafka and PostgreSQL

EDUCATION
B.S. Computer Science, University of Texas at Austin, 2015 - 2019

SKILLS
Python, Golang, PostgreSQL, Kafka, Redis, AWS, Docker, Node JS

PROJECTS
OpenLedger - Open source double-entry ledger library. github.com/jellis/openledger

CERTIFICATIONS
AWS Certified Solutions Architect - Associate, Amazon, 2023
"""

SAMPLE_JD_TEXT = (
    "Senior Backend Engineer with experience in distributed systems, "
    "Python or Go, PostgreSQL, and cloud deployment on AWS."
)

def create_dummy_pdf(path="dummy.pdf"):
    doc = fitz.open()
    page = doc.new_page()
    page.insert_text((50, 50), SAMPLE_RESUME_TEXT, fontsize=10, fontname="helv")
    doc.save(path)
    doc.close()
    return Path(path)

def measure():
    process = psutil.Process(os.getpid())
    gc.collect()
    start_mem = process.memory_info().rss / 1024 / 1024

    pdf_path = create_dummy_pdf()
    
    metrics = {}
    
    # Force Mock LLM so we don't spam Groq API during benchmarks
    os.environ["MOCK_LLM"] = "1"
    
    pipeline = InferencePipeline()
    t0 = time.perf_counter()
    result = pipeline.run(str(pdf_path), SAMPLE_JD_TEXT)
    t1 = time.perf_counter()
    
    metrics["total_inference_time"] = t1 - t0
    
    # Overall
    end_mem = process.memory_info().rss / 1024 / 1024
    metrics["peak_memory_mb"] = end_mem - start_mem
    metrics["cpu_percent"] = psutil.cpu_percent(interval=1.0)
    
    import json
    print("INTERNAL METRICS (C++ Pipeline):")
    print(json.dumps(metrics, indent=2))
    
    if os.path.exists("dummy.pdf"):
        os.remove("dummy.pdf")

if __name__ == "__main__":
    measure()
