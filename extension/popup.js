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

  // Profile elements
  const fileUploadGroup = document.getElementById('fileUploadGroup');
  const profileStatusIcon = document.getElementById('profileStatusIcon');
  const profileStatusText = document.getElementById('profileStatusText');
  const saveProfileBtn = document.getElementById('saveProfileBtn');
  const clearProfileBtn = document.getElementById('clearProfileBtn');

  let savedResumeText = null;

  // Load profile on start
  chrome.storage.local.get(['resumeText'], (result) => {
    if (result.resumeText) {
      setProfileActive(result.resumeText);
    }
  });

  function setProfileActive(text) {
    savedResumeText = text;
    profileStatusIcon.textContent = '✅';
    profileStatusText.textContent = 'Profile saved';
    fileUploadGroup.style.display = 'none';
    clearProfileBtn.style.display = 'inline-block';
    saveProfileBtn.textContent = 'Update Profile';
  }

  clearProfileBtn.addEventListener('click', () => {
    chrome.storage.local.remove(['resumeText'], () => {
      savedResumeText = null;
      profileStatusIcon.textContent = '❌';
      profileStatusText.textContent = 'No resume saved';
      fileUploadGroup.style.display = 'block';
      clearProfileBtn.style.display = 'none';
      saveProfileBtn.textContent = 'Save Page as Resume';
      resumeUpload.value = '';
    });
  });

  saveProfileBtn.addEventListener('click', async () => {
    try {
      let [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
      if (!tab) return;
      
      chrome.scripting.executeScript({
        target: { tabId: tab.id },
        func: extractTextFromPage
      }, (results) => {
        if (chrome.runtime.lastError) {
          showToast("Error: " + chrome.runtime.lastError.message);
          return;
        }
        if (results && results[0] && results[0].result) {
          const text = results[0].result;
          if (text.startsWith && text.startsWith("ERROR:")) {
             showToast("Extract Error: " + text.substring(0, 50));
             return;
          }
          if (text.length > 50) {
             chrome.storage.local.set({ resumeText: text }, () => {
               setProfileActive(text);
               showToast("Resume profile saved successfully!");
             });
          } else {
             showToast("Not enough text on page to save.");
          }
        } else {
          showToast("Failed to extract text from this page.");
        }
      });
    } catch (e) {
      showToast("Error saving profile: " + e.message);
    }
  });

  // Try to auto-extract JD on load
  autoExtractJD();

  autoExtractBtn.addEventListener('click', autoExtractJD);

  jobDescriptionInput.addEventListener('input', (e) => {
    charCountDisplay.textContent = `${e.target.value.length} / 1500`;
  });

  scoreForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    
    const file = resumeUpload.files[0];
    const jdText = jobDescriptionInput.value;

    if (!savedResumeText && !file) {
      showToast("Please provide a PDF or save a profile first.");
      return;
    }
    
    if (!jdText) {
      showToast("Please provide a Job Description.");
      return;
    }

    if (!savedResumeText && file && file.size > 2 * 1024 * 1024) {
      showToast("File is too large! Please upload a PDF under 2MB.");
      return;
    }

    submitBtn.disabled = true;
    btnText.textContent = "Analyzing...";

    const formData = new FormData();
    if (savedResumeText) {
      formData.append('resume_text', savedResumeText);
    } else {
      formData.append('pdf_file', file);
    }
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
      
      const rememberResume = document.getElementById('rememberResume');
      if (!savedResumeText && rememberResume && rememberResume.checked && data.parsed_resume && data.parsed_resume.raw_extracted_text) {
        const text = data.parsed_resume.raw_extracted_text;
        chrome.storage.local.set({ resumeText: text }, () => {
          setProfileActive(text);
          showToast("Perfectly extracted profile saved from PDF!");
        });
      }

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
        if (chrome.runtime.lastError) {
          console.error("Injection error:", chrome.runtime.lastError.message);
          return;
        }
        if (results && results[0] && results[0].result) {
          const text = results[0].result;
          if (text.startsWith && text.startsWith("ERROR:")) {
             console.error(text);
             return;
          }
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
  async function extractTextFromPage() {
    try {
      // 1. Google Docs specific extraction using the official Export API!
      if (window.location.hostname.includes('docs.google.com')) {
        const match = window.location.pathname.match(/\/d\/([a-zA-Z0-9-_]+)/);
        if (match && match[1]) {
          const docId = match[1];
          try {
            // Fetch the pristine text document directly from Google Docs
            const response = await fetch(`https://docs.google.com/document/d/${docId}/export?format=txt`);
            if (response.ok) {
              const text = await response.text();
              if (text && text.trim().length > 50) {
                return text.trim();
              }
            }
          } catch (e) {
            console.log("Failed to fetch doc text, falling back...", e);
          }
        }
        
        // Fallback if export fails
        let text = document.body ? document.body.innerText : "";
        if (!text) {
          const editor = document.querySelector('.kix-appview-editor') || document.querySelector('#kix-appview');
          if (editor) text = editor.innerText;
        }

        if (text) {
          const uiGarbage = [
            /FileEditViewInsertFormatTools.*?Help/g,
            /Normal text/g,
            /Calibri/g,
            /Arial/g,
            /Editing/g,
            /Show tabs and outlines/g,
            /Turn on screen reader support/g,
            /To enable screen reader support.*?Ctrl\+slash/g,
            /Banner hidden/g,
            /^[\s\d]+$/gm // Remove ruler numbers
          ];
          for (let regex of uiGarbage) {
            text = text.replace(regex, "");
          }
          return text.trim();
        }
      }

      // 2. Try to find common JD containers (LinkedIn, Indeed, etc)
      const selectors = [
        '#job-details', // LinkedIn split view
        '.jobs-description__content', // LinkedIn standalone
        '.jobs-search__job-details--container', // LinkedIn search right pane
        '.job-view-layout', // LinkedIn alternate
        '.job-description', 
        '#jobDescriptionText', // Indeed
        '.jobDescriptionContent',
        'div[data-testid="job-description"]' // Modern job boards
      ];
      
      for (let s of selectors) {
        const el = document.querySelector(s);
        if (el && el.innerText && el.innerText.length > 50) {
           return el.innerText.trim();
        }
      }
      
      // 3. Fallback: Just grab the body text
      if (document.body && document.body.innerText) {
        return document.body.innerText.trim();
      }
      return "";
    } catch (e) {
      return "ERROR: " + e.message;
    }
  }
});
