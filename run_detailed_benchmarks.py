import asyncio
import aiohttp
import time
import statistics
import json
import os
from fpdf import FPDF

# Generate valid PDF
pdf = FPDF()
pdf.add_page()
pdf.set_font("Arial", size=12)
pdf.cell(200, 10, txt="Sample Resume User. Engineer with python skills.", ln=1, align='C')
pdf.output("dummy.pdf")
with open("dummy.pdf", "rb") as f:
    dummy_pdf = f.read()

API_URL = "http://localhost:8000/api/score"
dummy_jd = "Software Engineer with Python experience."

async def fetch(session, sem):
    async with sem:
        start = time.perf_counter()
        data = aiohttp.FormData()
        data.add_field('pdf_file', dummy_pdf, filename='dummy.pdf', content_type='application/pdf')
        data.add_field('job_description', dummy_jd)
        
        try:
            async with session.post(API_URL, data=data) as response:
                await response.read()
                status = response.status
        except Exception as e:
            status = 500
        end = time.perf_counter()
        return end - start, status

async def benchmark_concurrency(concurrency, total_requests):
    sem = asyncio.Semaphore(concurrency)
    async with aiohttp.ClientSession() as session:
        tasks = [fetch(session, sem) for _ in range(total_requests)]
        
        t0 = time.perf_counter()
        results = await asyncio.gather(*tasks)
        t1 = time.perf_counter()
        
        total_time = t1 - t0
        latencies = [r[0] for r in results if r[1] == 200]
        failures = len([r for r in results if r[1] != 200])
        
        if latencies:
            avg_lat = statistics.mean(latencies)
            med_lat = statistics.median(latencies)
            p95 = sorted(latencies)[int(len(latencies)*0.95)] if len(latencies) > 0 else 0
            p99 = sorted(latencies)[int(len(latencies)*0.99)] if len(latencies) > 0 else 0
            max_lat = max(latencies)
            min_lat = min(latencies)
        else:
            avg_lat = med_lat = p95 = p99 = max_lat = min_lat = 0
            
        throughput = total_requests / total_time
        
        return {
            "concurrency": concurrency,
            "total_time": total_time,
            "throughput": throughput,
            "avg_latency": avg_lat,
            "med_latency": med_lat,
            "p95": p95,
            "p99": p99,
            "max": max_lat,
            "min": min_lat,
            "failures": failures,
            "success_rate": ((total_requests - failures) / total_requests) * 100
        }

async def main():
    print("Running API Benchmark (100 requests)...")
    base_bench = await benchmark_concurrency(10, 100)
    print("API Benchmark (100 reqs):", json.dumps(base_bench, indent=2))
    
    print("\nRunning Stress Test...")
    for c in [10, 25, 50, 100]:
        print(f"Testing {c} concurrent users...")
        res = await benchmark_concurrency(c, c)
        print(f"Result for {c}:", json.dumps(res, indent=2))

if __name__ == "__main__":
    asyncio.run(main())
