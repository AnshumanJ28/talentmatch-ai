# TalentMatch AI

**Resume ↔ Job Description ATS Match Scorer**

🌐 **Live Demo:** [https://talentmatch-ai-orpin.vercel.app/](https://talentmatch-ai-orpin.vercel.app/)

Upload a resume PDF and paste a job description — get back a 0–1 match score, a plain-English explanation of the strongest signals, and a breakdown of matched, missing, and partially-matched skills.

---

## Overview

TalentMatch AI simulates how an Applicant Tracking System (ATS) evaluates a candidate against a job posting. Instead of a black-box score, it surfaces *why* a resume matches (or doesn't) — the experience signals, skill overlaps, and feature-level scoring that drove the result — so both recruiters and candidates can understand the outcome.

## How It Works

1. **Parse** — The resume PDF is parsed and structured using an LLM (Groq).
2. **Extract skills** — Skills are extracted and normalized against a canonical taxonomy.
3. **Engineer features** — Experience, seniority, education, and skill-match signals are computed.
4. **Embed** — Resume and job description text are embedded into vector space.
5. **Rank** — A heuristic ranker combines embedding similarity with the engineered features into a single 0–1 score.
6. **Explain** — A narrative summary and feature breakdown are generated alongside the score.

## Project Structure

```
src/
├── parsing/          # PDF extraction + LLM-based resume structuring
├── skills/           # Skill extraction and taxonomy matching
├── features/         # Feature engineering (experience, education, skill ratios)
├── embeddings/        # Resume/JD text embedding generation
├── ranking/           # HeuristicRanker — combines embeddings + features into a score
├── explainability/     # Human-readable narrative summaries
├── pipeline.py         # Wires all stages together end-to-end
├── config.py            # Environment/config loading (API keys, etc.)
└── devices.py            # Device management (CPU/GPU)
tests/
└── test_pipeline.py       # End-to-end pipeline test
app.py                       # Gradio web UI — single entry point for the app
Dockerfile / docker-compose.yml   # Container build + run config
requirements.txt              # Python dependencies
```

## Architecture Notes

- **Single source of truth** — `DeviceManager`, `ProjectConfig`, `DirectoryManager`, secrets access, and `get_logger` each have exactly one canonical implementation, in `src/devices.py` and `src/config.py`.
- **Modular pipeline** — Parsing, skill extraction, feature engineering, embeddings, ranking, and explainability are separate, testable modules under `src/`, wired together by `src/pipeline.py`.
- **Ranking (v1)** — `src/ranking/ranker.py`'s `HeuristicRanker` is the sole ranker: **60% cosine similarity** between resume and JD embeddings, **40% spread across fused numeric features**. No vector index or learning-to-rank model is used — a single upload-one-resume/score-against-one-JD flow doesn't need one.

## Getting Started

### Local Development

```bash
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
export GROQ_API_KEY=your_key
python -m pytest tests/ -v   # optional: run the test suite first
python app.py
```

Open **http://localhost:7860** (Gradio's default port).

> **Windows users:** activate the venv with `.venv\Scripts\activate` instead, and set the key with `$env:GROQ_API_KEY="your_key"` (PowerShell) or via a `.env` file (see below).

### Using a `.env` File (recommended)

```bash
cp .env.example .env   # then fill in your real GROQ_API_KEY
```

The app and tests will automatically load this file — no need to `export`/`set` the key manually each session.

### Containerized (Docker)

```bash
cp .env.example .env   # fill in your real GROQ_API_KEY
docker build -t talentmatch .
docker compose up
```

## Requirements

- Python 3.11+
- A [Groq API key](https://console.groq.com/keys) (free tier available)

## Documentation

See `TalentMatch-AI-Execution-Plan.md` for the full stage-by-stage build plan this repo was implemented from.

## Contributors

- **Anshuman Pandey** – Machine Learning pipeline, model integration, Gradio UI, and Docker containerization.
- **Arnav Shukla** – Hosting, deployment, live demo setup, Edge extension integration, and integration work across the app.

## License

No license has been added to this repository yet. Add a `LICENSE` file (e.g. MIT) if you intend for others to reuse the code.
