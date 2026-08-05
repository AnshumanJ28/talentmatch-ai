from __future__ import annotations

from typing import List, Optional

from pydantic import BaseModel, ConfigDict, Field

from src.config import utc_now_iso


class ExtractionMetadata(BaseModel):
    """Metadata about the PDF text extraction step (Python side only)."""
    model_config = ConfigDict(extra="ignore")

    source_file:   str
    extracted_at:  str = Field(default_factory=utc_now_iso)
    parser_version: str = "v2.0-deterministic"  # replaces llm_model_used
    page_count:    int = 0
    char_count:    int = 0


# ---------------------------------------------------------------------------
# The following Pydantic models are kept for backward compatibility with
# any code that still imports from src.parsing.schema, but they are no
# longer used by the v2 pipeline (all parsing happens in C++).
# ---------------------------------------------------------------------------

class ContactInfo(BaseModel):
    model_config = ConfigDict(extra="ignore")
    full_name:     Optional[str] = None
    email:         Optional[str] = None
    phone:         Optional[str] = None
    location:      Optional[str] = None
    linkedin_url:  Optional[str] = None
    github_url:    Optional[str] = None
    portfolio_url: Optional[str] = None


class ExperienceEntry(BaseModel):
    model_config = ConfigDict(extra="ignore")
    company:          Optional[str] = None
    job_title:        Optional[str] = None
    start_date:       Optional[str] = None
    end_date:         Optional[str] = None
    is_current:       Optional[bool] = None
    responsibilities: List[str] = Field(default_factory=list)


class EducationEntry(BaseModel):
    model_config = ConfigDict(extra="ignore")
    institution:    Optional[str] = None
    degree:         Optional[str] = None
    field_of_study: Optional[str] = None
    end_date:       Optional[str] = None
    gpa:            Optional[str] = None


class ResumeSchema(BaseModel):
    """Lightweight schema used only by Python tests and API validation."""
    model_config = ConfigDict(extra="ignore")
    raw_extracted_text: str = ""
    metadata: ExtractionMetadata = Field(default_factory=lambda: ExtractionMetadata(source_file="unknown"))
