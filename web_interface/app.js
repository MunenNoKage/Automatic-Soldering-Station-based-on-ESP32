/**
 * @file app.js
 * @brief Main application logic for index page
 *
 * Handles drill file upload, parsing, preview, and sending to controller.
 */

// DOM elements
let fileInput;
let uploadBtn;
let fileNameDisplay;
let uploadStatus;
let selectedFile = null;

// Preview elements
let previewSection;
let drillContentDisplay;
let gcodeContentDisplay;
let sendBtn;
let cancelBtn;
let sendStatus;

// Parsed data storage
let parsedGCode = null;
let originalDrillContent = null;

/**
 * Initialize the application
 */
document.addEventListener('DOMContentLoaded', function() {
    // Get DOM elements
    fileInput = document.getElementById('drill-file-input');
    uploadBtn = document.getElementById('upload-btn');
    fileNameDisplay = document.getElementById('file-name');
    uploadStatus = document.getElementById('upload-status');
    manualEnterBtn = document.getElementById('btn-manual');

    // Preview elements
    previewSection = document.getElementById('preview-section');
    drillContentDisplay = document.getElementById('drill-content');
    gcodeContentDisplay = document.getElementById('gcode-content');
    sendBtn = document.getElementById('send-btn');
    cancelBtn = document.getElementById('cancel-btn');
    sendStatus = document.getElementById('send-status');

    uploadBtn.disabled = true;

    // Add event listeners
    if (fileInput) {
        fileInput.addEventListener('change', handleFileSelect);
    }

    if (uploadBtn) {
        uploadBtn.addEventListener('click', handleFileParse);
    }

    if (sendBtn) {
        sendBtn.addEventListener('click', handleSendToController);
    }

    if (cancelBtn) {
        cancelBtn.addEventListener('click', handleCancel);
    }

    if (manualEnterBtn) {
        manualEnterBtn.addEventListener('click', handleManualEnter);
    }
});

/**
 * Handle file selection
 */
function handleFileSelect(event) {
    const file = event.target.files[0];

    if (!file) {
        selectedFile = null;
        uploadBtn.disabled = true;
        fileNameDisplay.textContent = '';
        hidePreview();
        return;
    }

    // Check file extension
    if (!file.name.endsWith('.DRL')) {
        uploadStatus.textContent = 'Error: Only .DRL files are accepted';
        uploadStatus.className = 'upload-status error';
        selectedFile = null;
        uploadBtn.disabled = true;
        fileNameDisplay.textContent = '';
        hidePreview();
        return;
    }

    // Valid file selected
    selectedFile = file;
    fileNameDisplay.textContent = file.name;
    uploadBtn.disabled = false;
    uploadBtn.textContent = 'Parse';
    uploadStatus.textContent = '';
    uploadStatus.className = 'upload-status';
    hidePreview();
}

/**
 * Handle file parsing (first button click)
 */
async function handleFileParse() {
    if (!selectedFile) {
        return;
    }

    uploadStatus.textContent = 'Processing drill file...';
    uploadStatus.className = 'upload-status info';
    uploadBtn.disabled = true;

    try {
        // Read file content
        const fileContent = await readFileContent(selectedFile);
        originalDrillContent = fileContent;

        // Parse drill file to G-Code
        uploadStatus.textContent = 'Converting to G-Code...';
        const gcode = parseDrillToGCode(fileContent);

        if (!gcode) {
            throw new Error('Failed to parse drill file');
        }

        parsedGCode = gcode;

        // Display G-Code statistics
        const lines = gcode.split('\n').filter(l => l.trim() && !l.trim().startsWith(';'));
        uploadStatus.textContent = `Successfully parsed! Generated ${lines.length} G-Code commands.`;
        uploadStatus.className = 'upload-status success';

        // Show preview
        showPreview(fileContent, gcode);

    } catch (error) {
        uploadStatus.textContent = `Error: ${error.message}`;
        uploadStatus.className = 'upload-status error';
        uploadBtn.disabled = false;
    }
}

/**
 * Handle sending G-Code to controller (second button click)
 */
async function handleSendToController() {
    if (!parsedGCode || !selectedFile) {
        return;
    }

    sendStatus.textContent = 'Uploading to controller...';
    sendStatus.className = 'upload-status info';
    sendBtn.disabled = true;

    try {
        // Strip comment lines and empty lines for ESP32
        const cleanGCode = parsedGCode
            .split('\n')
            .map(line => line.trim())
            .filter(line => line.length > 0 && !line.startsWith(';'))
            .join('\n');
        
        // Send clean G-Code to server
        const response = await fetch('/api/gcode/upload', {
            method: 'POST',
            headers: {
                'Content-Type': 'text/plain',
            },
            body: cleanGCode
        });

        if (response.ok) {
            const result = await response.json();
            const commandCount = cleanGCode.split('\n').length;
            sendStatus.textContent = `Success: G-Code uploaded to controller (${commandCount} commands). Calibrating...`;
            sendStatus.className = 'upload-status success';

            // Redirect to execution control page after a short delay
            setTimeout(() => {
                window.location.href = 'exec.html';
            }, 1500);

        } else {
            const error = await response.json();
            sendStatus.textContent = `Error: ${error.message || 'Upload failed'}`;
            sendStatus.className = 'upload-status error';
            sendBtn.disabled = false;
        }
    } catch (error) {
        sendStatus.textContent = `Error: ${error.message}`;
        sendStatus.className = 'upload-status error';
        sendBtn.disabled = false;
    }
}

/**
 * Handle cancel button
 */
function handleCancel() {
    resetUploadForm();
}

/**
 * Show preview section with content
 */
function showPreview(drillContent, gcode) {
    drillContentDisplay.textContent = drillContent;
    gcodeContentDisplay.textContent = gcode;
    previewSection.style.display = 'block';
    sendStatus.textContent = '';
    sendStatus.className = 'upload-status';
    sendBtn.disabled = false;

    // Scroll to preview
    previewSection.scrollIntoView({ behavior: 'smooth', block: 'start' });
}

/**
 * Hide preview section
 */
function hidePreview() {
    previewSection.style.display = 'none';
    parsedGCode = null;
    originalDrillContent = null;
}

/**
 * Reset upload form to initial state
 */
function resetUploadForm() {
    fileInput.value = '';
    fileNameDisplay.textContent = '';
    selectedFile = null;
    uploadBtn.disabled = true;
    uploadBtn.textContent = 'Parse';
    uploadStatus.textContent = '';
    uploadStatus.className = 'upload-status';
    hidePreview();
}

/**
 * Read file content as text
 */
function readFileContent(file) {
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.onload = (e) => resolve(e.target.result);
        reader.onerror = (e) => reject(new Error('Failed to read file'));
        reader.readAsText(file);
    });
}

/**
 * Parse Excellon drill file to G-Code
 * @param {string} drillContent - Raw drill file content
 * @returns {string} Generated G-Code
 */
function parseDrillToGCode(drillContent) {
    const lines = drillContent.split('\n');
    let gcode = [];
    let drillPoints = [];
    let isMetric = true;

    // Add header comments
    gcode.push('; G-Code generated from Excellon drill file');
    gcode.push('; Generated: ' + new Date().toISOString());
    gcode.push('; Soldering Station Automatic Controller');
    gcode.push('');

    // Parse drill file
    for (let line of lines) {
        line = line.trim();

        // Skip empty lines and comments
        if (!line || line.startsWith(';')) continue;

        // Check for metric/inch mode
        if (line.includes('METRIC')) {
            isMetric = true;
            continue;
        }
        if (line.includes('INCH')) {
            isMetric = false;
            continue;
        }

        // Parse coordinates (X, Y) - ignore tool information
        const coordMatch = line.match(/X([-\d.]+)Y([-\d.]+)/);
        if (coordMatch) {
            let x = parseFloat(coordMatch[1]);
            let y = parseFloat(coordMatch[2]);

            // Convert to mm if needed
            if (!isMetric) {
                x *= 25.4;  // inches to mm
                y *= 25.4;
            }

            drillPoints.push({ x: x, y: y });
        }
    }

    // Process each drill point as a solder point
    gcode.push(`; === Soldering Operations (${drillPoints.length} points) ===`);

    for (let i = 0; i < drillPoints.length; i++) {
        const point = drillPoints[i];

        gcode.push('');
        gcode.push(`; Point ${i + 1}/${drillPoints.length} at X${point.x.toFixed(2)} Y${point.y.toFixed(2)}`);

        // Move to position with safe height
        gcode.push(`G0 X${point.x.toFixed(3)} Y${point.y.toFixed(3)}`);

        // Feed solder
        gcode.push('; Solder the point');
        gcode.push(`S75`);
    }

    gcode.push('');
    gcode.push('; === End of Program ===');

    return gcode.join('\n');
}

/**
 * Handle manual control mode entry
 */
async function handleManualEnter() {
    sendStatus.textContent = 'Entering manual control mode...';
    sendStatus.className = 'upload-status';

    try {
        const response = await fetch('/api/manual/enter', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            }
        });

        const result = await response.json();

        if (result.success) {
            sendStatus.textContent = 'Manual control mode activated';
            sendStatus.className = 'upload-status success';
            
            // Enable move and exit buttons
            manualEnterBtn.disabled = true;
            manualMoveBtn.disabled = false;
            manualExitBtn.disabled = false;
        } else {
            sendStatus.textContent = 'Error: ' + (result.message || 'Failed to enter manual mode');
            sendStatus.className = 'upload-status error';
        }
    } catch (error) {
        sendStatus.textContent = 'Error: ' + error.message;
        sendStatus.className = 'upload-status error';
    }

    window.location.href='manual.html'
}
