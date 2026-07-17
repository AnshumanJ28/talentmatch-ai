// API Configuration
const API_URL = 'https://talentmatch-ai-jwqd.onrender.com/api/score'; // Change for production

// Elements
const scoreForm = document.getElementById('scoreForm');
const resumeUpload = document.getElementById('resumeUpload');
const dropArea = document.getElementById('dropArea');
const fileNameDisplay = document.getElementById('fileName');
const submitBtn = document.getElementById('submitBtn');
const btnText = document.querySelector('.btn-text');
const btnLoader = document.getElementById('btnLoader');
const inputSection = document.getElementById('inputSection');
const resultsSection = document.getElementById('resultsSection');
const resetBtn = document.getElementById('resetBtn');
const scoreCirclePath = document.getElementById('scoreCirclePath');
const scoreValue = document.getElementById('scoreValue');
const explanationContent = document.getElementById('explanationContent');
const toast = document.getElementById('toast');

// File input interactions
resumeUpload.addEventListener('change', (e) => {
  if (e.target.files.length > 0) {
    fileNameDisplay.textContent = e.target.files[0].name;
  } else {
    fileNameDisplay.textContent = '';
  }
});

// Drag and drop styles
['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
  dropArea.addEventListener(eventName, preventDefaults, false);
});

function preventDefaults(e) {
  e.preventDefault();
  e.stopPropagation();
}

['dragenter', 'dragover'].forEach(eventName => {
  dropArea.addEventListener(eventName, () => dropArea.classList.add('drag-over'), false);
});

['dragleave', 'drop'].forEach(eventName => {
  dropArea.addEventListener(eventName, () => dropArea.classList.remove('drag-over'), false);
});

dropArea.addEventListener('drop', (e) => {
  let dt = e.dataTransfer;
  let files = dt.files;
  resumeUpload.files = files;
  
  if (files.length > 0) {
    fileNameDisplay.textContent = files[0].name;
  }
});

// Character Counter for JD
const jobDescriptionInput = document.getElementById('jobDescription');
const charCountDisplay = document.getElementById('charCount');

jobDescriptionInput.addEventListener('input', (e) => {
  const currentLength = e.target.value.length;
  charCountDisplay.textContent = `${currentLength} / 1500`;
});

// Form submission
scoreForm.addEventListener('submit', async (e) => {
  e.preventDefault();
  
  const file = resumeUpload.files[0];
  const jdText = jobDescriptionInput.value;

  if (!file || !jdText) {
    showToast("Please provide both a PDF and a Job Description.");
    return;
  }

  // File size validation (limit to 2MB to save memory)
  if (file.size > 2 * 1024 * 1024) {
    showToast("File is too large! Please upload a PDF under 2MB.");
    return;
  }

  if (jdText.length > 1500) {
    showToast("Job Description is too long. Max 1500 characters.");
    return;
  }

  // Set loading state
  submitBtn.disabled = true;
  btnText.textContent = "Analyzing...";
  btnLoader.style.display = "block";

  const formData = new FormData();
  formData.append('pdf_file', file);
  formData.append('job_description', jdText);

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
    // Reset loading state
    submitBtn.disabled = false;
    btnText.textContent = "Analyze Match";
    btnLoader.style.display = "none";
  }
});

// Display Results
function showResults(data) {
  const score = data.score;
  const percentage = Math.round(score * 100);
  
  const semanticScore = data.raw_similarity || 0;
  // Semantic score could be mathematically -1 to 1, but normally 0 to 1 for these embeddings.
  // We'll normalize it just in case, but assuming it's 0-1 based on the output.
  const semanticPercentage = Math.round(Math.max(0, semanticScore) * 100);

  // Transition UI
  inputSection.style.display = 'none';
  resultsSection.style.display = 'block';
  
  const semanticCirclePath = document.getElementById('semanticCirclePath');
  const semanticValue = document.getElementById('semanticValue');

  // Animate Circular Progress
  setTimeout(() => {
    scoreCirclePath.setAttribute('stroke-dasharray', `${percentage}, 100`);
    semanticCirclePath.setAttribute('stroke-dasharray', `${semanticPercentage}, 100`);
    
    // Set color based on score
    if (percentage >= 75) {
      scoreCirclePath.style.stroke = 'var(--success-color)';
    } else if (percentage >= 50) {
      scoreCirclePath.style.stroke = 'var(--accent-color)';
    } else {
      scoreCirclePath.style.stroke = 'var(--danger-color)';
    }

    if (semanticPercentage >= 75) {
      semanticCirclePath.style.stroke = 'var(--success-color)';
    } else if (semanticPercentage >= 50) {
      semanticCirclePath.style.stroke = 'var(--accent-color)';
    } else {
      semanticCirclePath.style.stroke = 'var(--danger-color)';
    }
  }, 100);

  // Count up animation for number
  animateValue(scoreValue, 0, percentage, 1500);
  animateValue(semanticValue, 0, semanticPercentage, 1500);

  // Parse Analysis Grid (Strengths & Improvements)
  const analysisGrid = document.getElementById('analysisGrid');
  const strengthsList = document.getElementById('strengthsList');
  const improvementsList = document.getElementById('improvementsList');
  
  if (data.explanation && data.explanation.feature_values) {
    const fv = data.explanation.feature_values;
    const strengths = [];
    const improvements = [];
    
    if (fv.has_experience === 1) strengths.push(`Includes Work Experience (${fv.total_experience_months || 0} months)`);
    else improvements.push('Missing Work Experience section');

    if (fv.has_projects === 1) strengths.push(`Includes Projects (${fv.num_projects || 0} listed)`);
    else improvements.push('Missing Projects section');

    if (fv.num_degrees > 0) strengths.push(`Includes Education (${fv.num_degrees} degree${fv.num_degrees > 1 ? 's' : ''})`);
    else improvements.push('Missing Education details');

    if (fv.has_gpa === 1) strengths.push(`GPA is included on the resume`);
    
    if (fv.has_certifications === 1) strengths.push(`Includes Certifications (${fv.num_certifications || 0} listed)`);
    else improvements.push('Consider adding relevant Certifications');
    
    strengthsList.innerHTML = strengths.map(s => `<li><span class="check-icon">✓</span> ${s}</li>`).join('');
    improvementsList.innerHTML = improvements.map(s => `<li><span class="x-icon">✗</span> ${s}</li>`).join('');
    analysisGrid.style.display = 'grid';
  } else {
    analysisGrid.style.display = 'none';
  }

  // Parse Skills
  const skillsContainer = document.getElementById('skillsContainer');
  const matchedSkillsDiv = document.getElementById('matchedSkills');
  const missingSkillsDiv = document.getElementById('missingSkills');
  
  const formatSkill = (skill) => {
      if (!skill) return "";
      const known = {
          'c': 'C', 'r': 'R', 'cpp': 'C++', 'c++': 'C++', 'c#': 'C#',
          'javascript': 'JavaScript', 'typescript': 'TypeScript', 'html': 'HTML',
          'css': 'CSS', 'php': 'PHP', 'sql': 'SQL', 'mysql': 'MySQL',
          'postgresql': 'PostgreSQL', 'aws': 'AWS', 'gcp': 'GCP',
          'api': 'API', 'ui': 'UI', 'ux': 'UX', 'react': 'React', 'node': 'Node.js'
      };
      if (known[skill.toLowerCase()]) return known[skill.toLowerCase()];
      return skill.split(' ').map(w => w.charAt(0).toUpperCase() + w.slice(1).toLowerCase()).join(' ');
  };

  if (data.explanation && data.explanation.skill_gap) {
    skillsContainer.style.display = 'block';
    const matched = data.explanation.skill_gap.matched_skills || [];
    const missing = data.explanation.skill_gap.missing_skills || [];
    
    matchedSkillsDiv.innerHTML = matched.map(s => `<span class="skill-tag matched">${formatSkill(s)}</span>`).join('') || '<span class="skill-tag empty">None found</span>';
    missingSkillsDiv.innerHTML = missing.map(s => `<span class="skill-tag missing">${formatSkill(s)}</span>`).join('') || '<span class="skill-tag empty">None missing</span>';
  } else {
    skillsContainer.style.display = 'none';
  }

  // Format full JSON for developer details
  const explanationContent = document.getElementById('explanationContent');
  explanationContent.textContent = JSON.stringify(data.explanation || data, null, 2);
}

// Reset UI
resetBtn.addEventListener('click', () => {
  scoreForm.reset();
  fileNameDisplay.textContent = '';
  charCountDisplay.textContent = '0 / 1500';
  resultsSection.style.display = 'none';
  inputSection.style.display = 'block';
  scoreCirclePath.setAttribute('stroke-dasharray', '0, 100');
  
  const semanticCirclePath = document.getElementById('semanticCirclePath');
  if (semanticCirclePath) {
    semanticCirclePath.setAttribute('stroke-dasharray', '0, 100');
  }
});

// Utilities
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
