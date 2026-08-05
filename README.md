# TalentMatch AI v2

**Production-grade Resume Ranking Engine — No LLM, No API Key**

![No API Key Required](https://img.shields.io/badge/API%20Key-Not%20Required-brightgreen)
![Engine](https://img.shields.io/badge/scoring%20engine-C%2B%2B-34d399)
![License](https://img.shields.io/badge/license-MIT-blue)

> v2 replaces the Groq/LLM-based pipeline with a deterministic C++ ML engine. All scoring intelligence runs natively in C++. Python is responsible only for PDF text extraction and embedding generation.

🌐 **Live Demo:** [talentmatch-ai-o22y.vercel.app](https://talentmatch-ai-o22y.vercel.app/)

---

## Table of Contents

- [Why TalentMatch AI](#why-talentmatch-ai)
- [Architecture](#architecture)
- [Sample Output](#sample-output)
- [C++ Engine Core](#c-engine-core-cpp_core)
- [Building](#building)
- [Getting Started](#getting-started)
- [Training the XGBoost Model](#training-the-xgboost-model-optional)
- [What Was Removed (v1 → v2)](#what-was-removed-v1--v2)
- [Project Structure](#project-structure)
- [Contributors](#contributors)
- [License](#license)

---

## Why TalentMatch AI

Most "AI resume matchers" are a thin prompt wrapped around a hosted LLM — slow, non-deterministic, and dependent on a third-party API key and rate limit. TalentMatch AI v2 takes the opposite approach:

- **Deterministic** — the same resume and job description always produce the same score. No prompt drift, no hallucinated skills.
- **Local-first** — embeddings run on a local MiniLM model; scoring runs in a native C++ library. Nothing leaves the machine unless you choose to deploy it.
- **Fast** — Aho-Corasick trie matching, BM25 retrieval, and ~85 engineered features are computed natively instead of round-tripping to a hosted model.
- **Explainable** — every score ships with rule-based positive/negative factors instead of an opaque LLM narrative.
- **Zero-config** — no `.env` file, no API key, no external service dependency to get a working scorer out of the box.

---

## Architecture

```mermaid
%%{init: {'theme': 'base', 'themeVariables': {
    'primaryColor': '#1e293b',
    'primaryTextColor': '#f8fafc',
    'primaryBorderColor': '#38bdf8',
    'lineColor': '#94a3b8',
    'secondaryColor': '#1e1b4b',
    'tertiaryColor': '#0f172a',
    'background': 'transparent',
    'mainBkg': '#1e293b',
    'clusterBkg': 'transparent',
    'clusterBorder': '#475569'
}}}%%
graph TD
    %% Style Definitions
    classDef client fill:#1e293b,stroke:#38bdf8,stroke-width:2px,color:#f8fafc;
    classDef python fill:#1e1b4b,stroke:#818cf8,stroke-width:2px,color:#f8fafc;
    classDef cpp fill:#062f4f,stroke:#34d399,stroke-width:2px,color:#f8fafc;
    classDef data fill:#3f220f,stroke:#fbbf24,stroke-width:2px,color:#f8fafc;

    subgraph Client ["Client (Web UI / Extension)"]
        A["User Uploads Resume & JD"]:::client
        H["Render Match Report & ATS Score"]:::client
    end

    subgraph Python ["Python Orchestration Layer (FastAPI)"]
        B["PDF Parser (PyMuPDF / EasyOCR)"]:::python
        C["Embedder (Local MiniLM Model)"]:::python
        D["ctypes FFI JSON Bridge"]:::python
    end

    subgraph Cpp ["C++ Core ML Scorer (Native DLL)"]
        E1["Deterministic Resume Parser (Regex)"]:::cpp
        E2["Skill Engine (Aho-Corasick Trie)"]:::cpp
        E3["Text Retrieval Engine (BM25)"]:::cpp

        F["Feature Engineering (~85 Features)"]:::cpp

        G1["XGBoost ML Ranker (or Linear Fallback)"]:::cpp
        G2["Explanation Engine (Rule-based)"]:::cpp
    end

    subgraph ModelData ["Model & Config Assets"]
        M1[("xgboost_model.json")]:::data
        M2[("skill_taxonomy.json")]:::data
    end

    %% Flow Paths
    A --> B
    B -->|"Extract plain text"| C
    C -->|"Compute vector embeddings"| D

    D -->|"Send Raw Text & Embeddings"| E1
    D -->|"Send Raw Text & Embeddings"| E2
    D -->|"Send Raw Text & Embeddings"| E3

    M2 --> E2

    E1 -->|"Education & Experience features"| F
    E2 -->|"Skill overlap statistics"| F
    E3 -->|"Probabilistic text overlaps"| F

    F --> G1
    F --> G2

    M1 --> G1

    G1 -->|"Return ATS Score (0-100)"| D
    G2 -->|"Return Match Factors"| D

    D -->|"JSON Response"| H

    style Client fill:transparent,stroke:#38bdf8,stroke-width:1.5px
    style Python fill:transparent,stroke:#818cf8,stroke-width:1.5px
    style Cpp fill:transparent,stroke:#34d399,stroke-width:1.5px
    style ModelData fill:transparent,stroke:#fbbf24,stroke-width:1.5px
```

> **Note on rendering:** the diagram uses a `%%{init}%%` directive and transparent subgraph fills so it renders cleanly on both GitHub's light and dark themes. If you're viewing this in an editor that doesn't support Mermaid `init` directives, a static PNG export is available at `docs/architecture.png`.

**Python does NOT compute any score.** It only:
1. Extracts text from PDF (PyMuPDF)
2. Generates resume + JD embeddings (sentence-transformers, local)
3. Calls the C++ engine via ctypes
4. Returns the JSON response

Everything between the FFI boundary and the JSON response — parsing, skill matching, feature engineering, retrieval scoring, ranking, and explanation generation — runs natively in C++.

---

## Sample Output

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

## C++ Engine Core (`cpp_core`)

TalentMatch AI v2 compiles all ATS scoring, feature engineering, and machine learning scoring logic into a high-performance native C++ library. Python acts purely as a wrapper for high-level PDF text extraction, local embedding generation, and web routing, communicating with the core engine via a lightweight FFI bridge.

### Core Modules Directory Layout

```text
cpp_core/
├── CMakeLists.txt              # Configures CMake build settings and compiler optimizations.
├── engine.cpp                  # Entry API wrapper exposing standard C-linkage JSON interfaces.
│
├── explainability/             # Logic to generate user-facing match factors and reasons
│   ├── explanation_engine.cpp  # Implementation of positive indicators and missing skill warnings.
│   └── explanation_engine.hpp  # Interface header for explanations.
│
├── features/                   # Feature engineering and metric calculation modules
│   ├── education.cpp           # Scores academic degrees, institutional rankings, and tiers.
│   ├── education.hpp           # Education feature declaration headers.
│   ├── experience.cpp          # Scores timeline durations, transitions, and title seniority.
│   ├── experience.hpp          # Experience feature declaration headers.
│   ├── skills_features.cpp     # Calculates matched/missing keyword statistics and ratios.
│   └── skills_features.hpp     # Skill metrics evaluation headers.
│
├── include/                    # Shared structures and library interface files
│   ├── engine.h                # Declares DLL exports for Python ctypes.
│   ├── features.hpp            # Struct definition for the ~85 feature vector.
│   ├── resume.hpp              # Internal schema representing the parsed candidate profile.
│   └── taxonomy.hpp            # Structures mapping skills and synonym taxonomy maps.
│
├── parser/                     # Deterministic resume parsing and structure recognition
│   ├── resume_parser.cpp       # Core parsing routines for dates, contact info, and segments.
│   ├── resume_parser.hpp       # Parsing orchestrator headers.
│   ├── section_detector.cpp    # Scans headings using regex to identify document regions.
│   └── section_detector.hpp    # Region detector headers.
│
├── ranking/                    # XGBoost rank evaluation logic
│   ├── xgboost_ranker.cpp      # Performs decision-tree calculations (falls back to linear weights).
│   └── xgboost_ranker.hpp      # Ranker fallback headers.
│
├── retrieval/                  # Classical text index queries
│   ├── bm25.cpp                # Computes probabilistic TF-IDF and BM25 relevance scores.
│   └── bm25.hpp                # Text indexing and query score headers.
│
└── skills/                     # High-performance skill keyword matching
    ├── skill_engine.cpp        # Evaluates raw text, normalized skills, and required JD coverages.
    ├── skill_engine.hpp        # Core skill engine interface.
    ├── synonym_matcher.cpp     # Resolves variants (e.g. "JS" matches "JavaScript").
    ├── synonym_matcher.hpp     # Synonym lookup config headers.
    ├── trie.cpp                # Implements Aho-Corasick trie structures for parallel scanning.
    └── trie.hpp                # Trie node property definitions.
```

---

## Building

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
| `GROQ_API_KEY` environment variable | Not required |

---

## Project Structure

```text
talentmatch-ai-main/
├── api.py                          # FastAPI endpoint: validates inputs, manages PDF parsing, runs inference pipelines.
├── app.py                          # Gradio UI helper for local developer validation and testing.
├── Dockerfile                      # Standard Docker build configuration.
├── docker-compose.yml              # Local multi-container Docker composition bindings.
├── LICENSE                         # Project license terms (MIT).
├── pyproject.toml                  # Python package build configurations.
├── requirements.txt                # Python pip package dependencies list.
├── render.yaml                     # Deployment instructions for hosting on Render.
├── README.md                       # Comprehensive repository documentation.
├── TalentMatch-AI-Execution-Plan.md # System implementation plan (migration to C++ engine).
├── test_api.py                     # Script to test FastAPI endpoints locally with sample PDF.
├── process_screenshots.ps1         # PowerShell script to crop/resize demo screenshots.
│
├── cpp_core/                       # Core ML matching & scoring engine in C++
│   ├── CMakeLists.txt              # Configures CMake compiler options, sources, and flags.
│   ├── engine.cpp                  # Central FFI entry-point exposing C-linkage JSON interfaces.
│   │
│   ├── explainability/             # Explanation templates and feature summaries
│   │   ├── explanation_engine.cpp  # Implementation of match highlights and warnings.
│   │   └── explanation_engine.hpp  # Header interface declaring explainer methods.
│   │
│   ├── features/                   # Feature extraction modules
│   │   ├── education.cpp           # Computes academic degrees, duration checks, and school tiers.
│   │   ├── education.hpp           # Header declaring education metric formulas.
│   │   ├── experience.cpp          # Computes dates, intervals, title seniority, and durations.
│   │   ├── experience.hpp          # Header declaring experience scoring structures.
│   │   ├── skills_features.cpp     # Analyzes overlaps, density metrics, and missing requirements.
│   │   └── skills_features.hpp     # Header declaring skill evaluation methods.
│   │
│   ├── include/                    # Core header files
│   │   ├── engine.h                # Declares FFI endpoints mapped via Python ctypes.
│   │   ├── features.hpp            # Structure schemas representing the ~85 feature vector.
│   │   ├── resume.hpp              # Structural model representing parsed profiles.
│   │   └── taxonomy.hpp            # Taxonomy map schemas and categorization.
│   │
│   ├── parser/                     # Deterministic resume text parsing
│   │   ├── resume_parser.cpp       # Matches structure layouts like contact info and dates.
│   │   ├── resume_parser.hpp       # Header declaring parsing workflows.
│   │   ├── section_detector.cpp    # Identifies limits of document sections using regex.
│   │   └── section_detector.hpp    # Header declaring section scanner methods.
│   │
│   ├── ranking/                    # XGBoost rank evaluation
│   │   ├── xgboost_ranker.cpp      # Performs decision-tree prediction. Falls back to linear weights.
│   │   └── xgboost_ranker.hpp      # Header declaring rank scoring fallbacks.
│   │
│   ├── retrieval/                  # Classical keyword indexing
│   │   ├── bm25.cpp                # Evaluates text overlap ratios with BM25 probabilistic weights.
│   │   └── bm25.hpp                # Header declaring term-frequency indexing schemas.
│   │
│   └── skills/                     # Skill matching and normalisation
│       ├── skill_engine.cpp        # Manages token matches and checks JD requirements coverage.
│       ├── skill_engine.hpp        # Header declaring skill matching classes.
│       ├── synonym_matcher.cpp     # Resolves variants (e.g. "Postgres" maps to "PostgreSQL").
│       ├── synonym_matcher.hpp     # Header declaring synonym dictionary maps.
│       ├── trie.cpp                # Builds and queries Trie structures (Aho-Corasick).
│       └── trie.hpp                # Header declaring Trie node properties.
│
├── src/                            # Python orchestration layer
│   ├── __init__.py                 # Initialization hooks.
│   ├── bridge.py                   # Loads native DLL via ctypes and manages memory-safe FFI calls.
│   ├── config.py                   # Central settings: paths, embedding batch sizes, and logging configurations.
│   ├── devices.py                  # Detects hardware runtimes (CUDA/MPS/CPU) for embeddings.
│   ├── pipeline.py                 # Core workflow: extracts text, gets embeddings, invokes FFI scorer.
│   │
│   ├── embeddings/                 # Local embedding computation
│   │   ├── __init__.py             # Module initialization.
│   │   └── generator.py            # Loads MiniLM transformers to calculate dense text vectors locally.
│   │
│   ├── explainability/             # (Legacy) Python explainer scripts
│   │   ├── __init__.py             # Module initialization.
│   │   └── explainer.py            # Legacy Python-side match explanation generator.
│   │
│   ├── features/                   # (Legacy) Python feature calculations
│   │   ├── __init__.py             # Module initialization.
│   │   └── engineering.py          # Legacy Python-side feature extraction.
│   │
│   ├── parsing/                    # PDF extraction and schemas
│   │   ├── __init__.py             # Module initialization.
│   │   ├── extractor.py            # Extracts digital PDFs via PyMuPDF or renders scanned pages via EasyOCR.
│   │   ├── llm_parser.py           # Legacy Groq/LLM-based parsing wrappers.
│   │   └── schema.py               # Serializes response datasets using Pydantic shapes.
│   │
│   ├── ranking/                    # (Legacy) Python model fallbacks
│   │   ├── __init__.py             # Module initialization.
│   │   └── ranker.py               # Legacy Python-side model weighting classes.
│   │
│   └── skills/                     # (Legacy) Python skill parsers
│       ├── __init__.py             # Module initialization.
│       ├── extractor.py            # Legacy Python skill scanner.
│       └── taxonomy.py             # Legacy taxonomy mapping profiles.
│
├── frontend/                       # Web user interface (Vite)
│   ├── index.html                  # Main markup holding cards, drag-and-drop zones, and score charts.
│   ├── style.css                   # Responsive CSS styles, layout custom grids, and dark theme variables.
│   ├── main.js                     # Listens to uploads, updates DOM, handles UI states and API queries.
│   ├── package.json                # Declares Vite configurations and package dependencies.
│   └── src/                        # Standard Vite boilerplate files
│       ├── counter.js              # Vite boilerplate counter utility.
│       ├── main.js                 # Boilerplate main script entry.
│       └── style.css               # Default boilerplate styles.
│
├── extension/                      # Chrome browser extension
│   ├── manifest.json               # Registers settings, title, actions, and tab query permissions.
│   ├── popup.html                  # Interface layout rendered inside browser extension popups.
│   ├── popup.js                    # Extracts active tab JD, saves resume, and handles FFI API calls.
│   └── style.css                   # Custom styles matching the main web client theme.
│
├── scripts/                        # Automations and ML training
│   ├── build_engine.ps1            # Windows CMake build wrapper script.
│   ├── build_engine.sh             # Linux/macOS CMake build shell script.
│   └── train_model.py              # Fits XGBoost classifier checkpoints onto synthesized candidate data.
│
├── models/                         # ML Model checkpoints
│   ├── xgboost_model.json          # Trained trees used for scoring candidates in the native engine.
│   └── xgboost_model.features.json # Sequences of vector features required by the models.
│
├── data/                           # Intermediate caches, data extracts, and configs
│
└── tests/                          # Automated tests
    └── test_pipeline.py            # End-to-end integration and FFI stability test suites.
```

---

## Contributors

- **Anshuman Pandey** – ML pipeline, C++ engine architecture, v2 refactor
- **Arnav Shukla** – Hosting, deployment, live demo, Edge extension

## License

See `LICENSE` for details.
