# TalentMatch AI v2

**Production-grade Resume Ranking Engine — No LLM, No API Key**

> v2 replaces the Groq/LLM-based pipeline with a deterministic C++ ML engine. All scoring intelligence runs natively in C++. Python is responsible only for PDF text extraction and embedding generation.

🌐 **Live Demo:** [talentmatch-ai-se48.vercel.app](https://talentmatch-ai-o22y.vercel.app/)

---

## Architecture

```
Frontend
    ↓
FastAPI (Python)
    ↓  receives upload, validates, generates embeddings
TalentMatch Core (C++)
    ↓  ctypes FFI boundary
    ├─ Resume Parser     (deterministic regex, no LLM)
    ├─ Skill Engine      (Aho-Corasick trie + fuzzy matching)
    ├─ Feature Engineering (~85 features across 5 categories)
    ├─ BM25 + TF-IDF     (retrieval scoring)
    ├─ Cosine Similarity (from Python-generated embeddings)
    ├─ XGBoost Ranker    (or linear fallback if model not trained)
    └─ Explanation Engine (deterministic rule-based, no LLM)
    ↓  JSON response
Frontend
```

**Python does NOT compute any score.** It only:
1. Extracts text from PDF (PyMuPDF)
2. Generates resume + JD embeddings (sentence-transformers, local)
3. Calls C++ engine via ctypes
4. Returns the JSON response

---

## Output

```json
{
  "overall_score": 91.3,
  "scores": {
    "skills":         95.0,
    "experience":     87.0,
    "education":     100.0,
    "projects":       90.0,
    "semantic":       93.0,
    "resume_quality": 82.0
  },
  "matched_skills":       ["Python", "PostgreSQL", "Docker", "Kafka"],
  "missing_skills":       ["Terraform"],
  "partial_skills":       [{"jd_skill": "cloud deployment", "matched_as": "AWS", "confidence": 0.71}],
  "top_positive_factors": [
    "Excellent skill match — 92% of required skills aligned",
    "Strong relevant experience — 6 yrs",
    "Strong semantic alignment with job description"
  ],
  "top_negative_factors": [
    "Missing required skills: Terraform",
    "Skill gap — only 87% of required skills found"
  ],
  "ranking_method": "linear_fallback"
}
```

---

## C++ Engine

### Module Overview

| Module | Location | Purpose |
|--------|----------|---------|
| Resume Parser | `cpp_core/parser/` | Deterministic regex parsing — extracts contact, experience, education, skills, projects, certifications |
| Skill Engine | `cpp_core/skills/` | Aho-Corasick trie + edit-distance fuzzy matching against 90+ canonical skills |
| Feature Engineering | `cpp_core/features/` | ~85 features: experience (15), education (10), projects (8), certifications (7), quality (10), skills (20), retrieval (15) |
| Retrieval | `cpp_core/retrieval/` | BM25, TF-IDF cosine, keyword overlap, bigram overlap |
| Ranking | `cpp_core/ranking/` | XGBoost (if trained) or deterministic linear fallback |
| Explainability | `cpp_core/explainability/` | Rule-based templates filled with actual feature values |

### Building

**Linux / macOS:**
```bash
chmod +x scripts/build_engine.sh
./scripts/build_engine.sh
```

**Windows (PowerShell):**
```powershell
# Requires Visual Studio 2022 with C++ workload + CMake
.\scripts\build_engine.ps1
```

**Docker (recommended):**
```bash
docker build -t talentmatch .
docker compose up
```

Manual CMake:
```bash
cd cpp_core
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

---

## Getting Started

### Local Development (no Docker)

```bash
# 1. Build the C++ engine (required)
./scripts/build_engine.sh

# 2. Python setup
python -m venv .venv
source .venv/bin/activate   # Windows: .venv\Scripts\activate
pip install -r requirements.txt

# 3. Run
uvicorn api:app --host 0.0.0.0 --port 8000

# Or Gradio UI:
python app.py
```

No `.env` file or API key needed.

### Run Tests

```bash
python -m pytest tests/ -v --no-header
```

Tests automatically skip C++ engine tests if the library hasn't been built yet.

---

## Training the XGBoost Model (Optional)

The engine ships with a **deterministic linear fallback** scorer that works out of the box.
To enable ML ranking via XGBoost:

```bash
# Install training dependencies
pip install xgboost scikit-learn

# Generate synthetic data and train (3000 samples by default)
python scripts/train_model.py

# With real labeled data (when available):
# python scripts/train_model.py --samples 10000 --output models/xgboost_model.json

# Rebuild with XGBoost support
./scripts/build_engine.sh --xgboost
```

---

## What Was Removed (v1 → v2)

| v1 | v2 |
|----|-----|
| Groq API (`llama-3.3-70b`) for resume parsing | Deterministic C++ regex parser |
| LLM skill extraction | Aho-Corasick trie + fuzzy matching |
| `HeuristicRanker` (60/40 hardcoded weights) | XGBoost (or linear fallback) |
| AI-generated narrative explanations | Rule-based template explanations |
| `easyocr` for image PDFs | PyMuPDF native text extraction |
| `groq`, `tenacity`, `httpx` dependencies | Removed entirely |
| GROQ_API_KEY environment variable | Not required |

---

## Project Structure

```
talentmatch-ai/
├── api.py                    # FastAPI entry point (thin wrapper)
├── app.py                    # Gradio UI shim
│
├── cpp_core/                 # Native C++ engine
│   ├── CMakeLists.txt
│   ├── engine.cpp            # C-linkage entry point (JSON in/out)
│   ├── include/              # Headers: engine.h, resume.hpp, features.hpp, taxonomy.hpp
│   ├── parser/               # section_detector + resume_parser
│   ├── skills/               # trie (Aho-Corasick), synonym_matcher, skill_engine
│   ├── features/             # experience, education, projects, certs, quality, skills
│   ├── retrieval/            # bm25 + tfidf
│   ├── ranking/              # xgboost_ranker (with linear fallback)
│   └── explainability/       # explanation_engine
│
├── src/                      # Python layer (orchestration only)
│   ├── bridge.py             # ctypes FFI to C++ library
│   ├── pipeline.py           # PDF extraction + embeddings + bridge call
│   ├── config.py             # Configuration (no Groq)
│   ├── devices.py            # CPU/GPU detection
│   ├── embeddings/           # sentence-transformers wrapper
│   └── parsing/              # schema.py (backward compat), extractor.py (PyMuPDF)
│
├── models/
│   └── xgboost_model.json    # Trained model (generated by scripts/train_model.py)
│
├── scripts/
│   ├── build_engine.sh       # Linux/macOS build
│   ├── build_engine.ps1      # Windows build
│   └── train_model.py        # XGBoost training
│
├── tests/
│   └── test_pipeline.py      # End-to-end + bridge tests (no network required)
│
├── requirements.txt          # Python deps (no groq, no easyocr)
└── Dockerfile                # Multi-stage: C++ build → Python runtime
```

---

## Contributors

- **Anshuman Pandey** – ML pipeline, C++ engine architecture, v2 refactor
- **Arnav Shukla** – Hosting, deployment, live demo, Edge extension

## License

See `LICENSE` for details.
