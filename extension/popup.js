const API_URL = 'https://talentmatch-ai-jwqd.onrender.com/api/score';

document.addEventListener('DOMContentLoaded', () => {
  const scoreForm = document.getElementById('scoreForm');
  const resumeUpload = document.getElementById('resumeUpload');
  const jobDescriptionInput = document.getElementById('jobDescription');
  const submitBtn = document.getElementById('submitBtn');
  const btnText = document.querySelector('.btn-text');
  const inputSection = document.getElementById('inputSection');
  const resultsSection = document.getElementById('resultsSection');
  const resetBtn = document.getElementById('resetBtn');
  const scoreCirclePath = document.getElementById('scoreCirclePath');
  const scoreValue = document.getElementById('scoreValue');
  const toast = document.getElementById('toast');
  const charCountDisplay = document.getElementById('charCount');
  const autoExtractBtn = document.getElementById('autoExtractBtn');

  // Try to auto-extract text on load
  autoExtractJD();

  autoExtractBtn.addEventListener('click', autoExtractJD);

  jobDescriptionInput.addEventListener('input', (e) => {
    charCountDisplay.textContent = `${e.target.value.length} / 1500`;
  });

  scoreForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    
    const file = resumeUpload.files[0];
    const jdText = jobDescriptionInput.value;

    if (!file || !jdText) {
      showToast("Please provide both a PDF and a Job Description.");
      return;
    }

    if (file.size > 2 * 1024 * 1024) {
      showToast("File is too large! Please upload a PDF under 2MB.");
      return;
    }

    submitBtn.disabled = true;
    btnText.textContent = "Analyzing...";

    const formData = new FormData();
    formData.append('pdf_file', file);
    formData.append('job_description', jdText.substring(0, 1500)); // enforce limit

    try {
      const response = await fetch(API_URL, {
        method: 'POST',
        body: formData,
      });

      if (!response.ok) {
        const errData = await response.json();
        throw new Error(errData.detail || 'Server error occurred');
      }

      const data = await response.json();
      showResults(data);
    } catch (error) {
      showToast(error.message);
    } finally {
      submitBtn.disabled = false;
      btnText.textContent = "Analyze Match";
    }
  });

  resetBtn.addEventListener('click', () => {
    scoreForm.reset();
    resultsSection.style.display = 'none';
    inputSection.style.display = 'block';
    scoreCirclePath.setAttribute('stroke-dasharray', '0, 100');
    charCountDisplay.textContent = '0 / 1500';
    autoExtractJD(); // Try to get text again
  });

  function showResults(data) {
    const score = data.score;
    const percentage = Math.round(score * 100);
    
    inputSection.style.display = 'none';
    resultsSection.style.display = 'block';
    
    setTimeout(() => {
      scoreCirclePath.setAttribute('stroke-dasharray', `${percentage}, 100`);
      
      if (percentage >= 75) {
        scoreCirclePath.style.stroke = 'var(--success-color)';
      } else if (percentage >= 50) {
        scoreCirclePath.style.stroke = 'var(--accent-color)';
      } else {
        scoreCirclePath.style.stroke = 'var(--danger-color)';
      }
    }, 100);

    animateValue(scoreValue, 0, percentage, 1500);
  }

  function showToast(message) {
    toast.textContent = message;
    toast.classList.add('show');
    setTimeout(() => {
      toast.classList.remove('show');
    }, 4000);
  }

  function animateValue(obj, start, end, duration) {
    let startTimestamp = null;
    const step = (timestamp) => {
      if (!startTimestamp) startTimestamp = timestamp;
      const progress = Math.min((timestamp - startTimestamp) / duration, 1);
      obj.innerHTML = Math.floor(progress * (end - start) + start) + "%";
      if (progress < 1) {
        window.requestAnimationFrame(step);
      }
    };
    window.requestAnimationFrame(step);
  }

  // --- Auto-Extraction Logic ---
  async function autoExtractJD() {
    try {
      let [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
      if (!tab) return;
      
      // Inject script to extract text
      chrome.scripting.executeScript({
        target: { tabId: tab.id },
        func: extractTextFromPage
      }, (results) => {
        if (results && results[0] && results[0].result) {
          const text = results[0].result;
          if (text.length > 50) { // arbitrary minimum
             jobDescriptionInput.value = text.substring(0, 1500);
             charCountDisplay.textContent = `${jobDescriptionInput.value.length} / 1500`;
          }
        }
      });
    } catch (e) {
      console.log("Auto-extract failed:", e);
    }
  }

  // This runs IN THE CONTEXT OF THE WEBPAGE
  function extractTextFromPage() {
    // 1. Google Docs specific extraction
    if (window.location.hostname.includes('docs.google.com')) {
      const paragraphs = document.querySelectorAll('.kix-paragraphrenderer');
      if (paragraphs.length > 0) {
        let docsText = '';
        paragraphs.forEach(p => {
          docsText += p.innerText + '\\n';
        });
        return docsText.trim();
      }
    }

    // 2. Try to find common JD containers (LinkedIn, Indeed, etc)
    const selectors = [
      '#job-details', // LinkedIn
      '.job-description', 
      '#jobDescriptionText', // Indeed
      '.jobDescriptionContent'
    ];
    
    for (let s of selectors) {
      const el = document.querySelector(s);
      if (el) return el.innerText;
    }
    
    // 3. Fallback: Just grab the body text
    return document.body.innerText;
  }
});
