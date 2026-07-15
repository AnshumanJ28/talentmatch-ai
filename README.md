# TalentMatch AI

Resume ↔ job-description ATS match scorer. Upload a resume PDF and paste a
job description; get back a 0-1 match score plus an explanation (top
scoring signals, matched/missing/partial skill matches).

This is the containerized, de-Colab'd version of the original
`Projects.ipynb` notebook, built per `TalentMatch-AI-Execution-Plan.md`.

## What changed vs. the notebook

- No Google Colab (`drive.mount`, `userdata.get`, `files.upload`) anywhere.
- `DeviceManager`, `ProjectConfig`, `DirectoryManager`, secrets access, and
  `get_logger` — each redefined 3-6 times across the notebook's phase
  bootstrap cells — now have exactly one canonical version, in
  `src/devices.py` and `src/config.py`.
- Fixed the ordering bug where `deployment_summary = run_phase10()` was
  called one cell before `run_phase10` was defined.
- Phases 1-8 are modularized into `src/parsing`, `src/skills`,
  `src/features`, `src/embeddings`, `src/ranking`, `src/explainability`,
  wired together by `src/pipeline.py`.
- Phase 6 (FAISS retrieval) and the LightGBM ranker are dropped for v1 —
  a single upload-one-resume/score-against-one-JD flow doesn't need a
  vector index or a learning-to-rank model trained on candidate pools.
  `src/ranking/ranker.py`'s `HeuristicRanker` is the default (and only)
  ranker: 60% cosine similarity between resume and JD embeddings, 40%
  spread across the fused numeric features.
- Phases 9-10 (offline evaluation, model pickling/deployment packaging)
  are dropped from the live app; they were batch/offline tooling, not
  part of the request-response path.

## Local development

```bash
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
export GROQ_API_KEY=your_key
python -m pytest tests/ -v
python app.py
```

Open http://localhost:7860.

## Containerized

```bash
cp .env.example .env   # fill in your real GROQ_API_KEY
docker build -t talentmatch .
docker compose up
```

## Deploy to Hugging Face Spaces

1. New Space → SDK: Docker → Hardware: CPU basic (free).
2. Connect it to this GitHub repo, or `git push space main`.
3. Space Settings → Repository secrets → add `GROQ_API_KEY`.

Free CPU Spaces sleep after inactivity and take ~20-40s to cold-start on
the next visit — that's expected, not a bug.

## Project layout

See `TalentMatch-AI-Execution-Plan.md` for the full stage-by-stage plan
this repo was built from.
