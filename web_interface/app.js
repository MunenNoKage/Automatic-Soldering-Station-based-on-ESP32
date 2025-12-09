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
let navigationSection;

// Parsed data storage
let parsedGCode = null;
let originalDrillContent = null;
let drillPoints = [];
let excludedHoles = new Set();  // Track excluded hole indices

// Canvas elements
let boardCanvas = null;
let boardCtx = null;
let drillCountDisplay = null;
let boardDimensionsDisplay = null;

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
    gcodeSection = document.getElementById('gcode-section');
    navigationSection = document.getElementById('manual-section');
    
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

    // Canvas elements
    boardCanvas = document.getElementById('board-canvas');
    if (boardCanvas) {
        boardCtx = boardCanvas.getContext('2d');
        // Add mouse move listener for hover tooltips
        boardCanvas.addEventListener('mousemove', handleCanvasHover);
        // Add click listener to toggle hole exclusion
        boardCanvas.addEventListener('click', handleCanvasClick);
    }
    drillCountDisplay = document.getElementById('drill-count');
    boardDimensionsDisplay = document.getElementById('board-dimensions');
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
        // Use already regenerated G-Code (updated in real-time)
        const filteredGCode = parsedGCode;
        
        // Strip comment lines and empty lines for ESP32
        const cleanGCode = filteredGCode
            .split('\n')
            .map(line => line.trim())
            .filter(line => line.length > 0 && !line.startsWith(';'))
            .join('\n');
        
        // Store G-code in session storage for exec.html
        sessionStorage.setItem('uploadedGCode', filteredGCode);
        
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
            const excludedCount = excludedHoles.size;
            const statusMsg = excludedCount > 0 
                ? `Success: G-Code uploaded (${drillPoints.length - excludedCount}/${drillPoints.length} holes, ${excludedCount} excluded). Calibrating...`
                : `Success: G-Code uploaded to controller (${commandCount} commands). Calibrating...`;
            sendStatus.textContent = statusMsg;
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

    // Hide navigation section when preview is shown
    if (navigationSection) {
        navigationSection.style.display = 'none';
        gcodeSection.style.display = 'none';
    }

    // Visualize drill holes on canvas
    visualizeDrillHoles();

    // Update preview displays
    updatePreviewDisplays();

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
    excludedHoles.clear();
    
    // Show navigation section when preview is hidden
    if (navigationSection) {
        navigationSection.style.display = 'block';
        gcodeSection.style.display = 'block';
    }
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
    drillPoints = [];  // Store globally for visualization
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

            // Take absolute values
            x = Math.abs(x);
            y = Math.abs(y);

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
 * Regenerate G-Code excluding marked holes
 */
function regenerateGCodeWithExclusions() {
    let gcode = [];
    
    // Add header comments
    gcode.push('; G-Code generated from Excellon drill file');
    gcode.push('; Generated: ' + new Date().toISOString());
    gcode.push('; Soldering Station Automatic Controller');
    if (excludedHoles.size > 0) {
        gcode.push(`; Excluded holes: ${excludedHoles.size}`);
    }
    gcode.push('');

    // Filter out excluded holes
    const includedPoints = drillPoints.filter((_, index) => !excludedHoles.has(index));
    
    // Process each included drill point as a solder point
    gcode.push(`; === Soldering Operations (${includedPoints.length} points) ===`);

    for (let i = 0; i < includedPoints.length; i++) {
        const point = includedPoints[i];

        gcode.push('');
        gcode.push(`; Point ${i + 1}/${includedPoints.length} at X${point.x.toFixed(2)} Y${point.y.toFixed(2)}`);

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
 * Regenerate drill file content excluding marked holes
 */
function regenerateDrillFileWithExclusions() {
    if (!originalDrillContent) return '';

    const lines = originalDrillContent.split('\n');
    let newDrillContent = [];
    let drillLineIndex = 0;

    for (let line of lines) {
        const trimmedLine = line.trim();
        
        // Check if this line contains drill coordinates
        const coordMatch = trimmedLine.match(/X([-\d.]+)Y([-\d.]+)/);
        
        if (coordMatch) {
            // This is a drill coordinate line
            if (!excludedHoles.has(drillLineIndex)) {
                // Include this hole
                newDrillContent.push(line);
            } else {
                // Exclude this hole - add as comment
                newDrillContent.push('; [EXCLUDED] ' + line);
            }
            drillLineIndex++;
        } else {
            // Not a coordinate line, keep as-is
            newDrillContent.push(line);
        }
    }

    return newDrillContent.join('\n');
}

/**
 * Update preview displays with current exclusions
 */
function updatePreviewDisplays() {
    // Update drill file content
    const filteredDrillContent = regenerateDrillFileWithExclusions();
    drillContentDisplay.textContent = filteredDrillContent;

    // Update G-code content
    const filteredGCode = regenerateGCodeWithExclusions();
    gcodeContentDisplay.textContent = filteredGCode;

    // Update parsed G-code (used for upload)
    parsedGCode = filteredGCode;
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

/**
 * Visualize drill holes on canvas
 */
function visualizeDrillHoles() {
    if (!boardCtx || !drillPoints || drillPoints.length === 0) {
        return;
    }

    // Calculate board dimensions from drill points
    let minX = Infinity, maxX = -Infinity;
    let minY = Infinity, maxY = -Infinity;

    for (const point of drillPoints) {
        minX = Math.min(minX, point.x);
        maxX = Math.max(maxX, point.x);
        minY = Math.min(minY, point.y);
        maxY = Math.max(maxY, point.y);
    }

    // Add margin around the board (10mm on each side)
    const margin = 10;
    const boardWidth = (maxX - minX) + (2 * margin);
    const boardHeight = (maxY - minY) + (2 * margin);

    // Canvas dimensions
    const canvasWidth = boardCanvas.width;
    const canvasHeight = boardCanvas.height;

    // Calculate scale to fit board in canvas with padding
    const padding = 40;
    const scaleX = (canvasWidth - 2 * padding) / boardWidth;
    const scaleY = (canvasHeight - 2 * padding) / boardHeight;
    const scale = Math.min(scaleX, scaleY);

    // Calculate offset to center the board
    const scaledBoardWidth = boardWidth * scale;
    const scaledBoardHeight = boardHeight * scale;
    const offsetX = (canvasWidth - scaledBoardWidth) / 2;
    const offsetY = (canvasHeight - scaledBoardHeight) / 2;

    // Store visualization parameters globally for hover detection
    window.visualizationParams = {
        minX, minY, maxX, maxY,
        margin, scale,
        offsetX, offsetY,
        canvasWidth, canvasHeight,
        scaledBoardWidth, scaledBoardHeight
    };

    drawCanvas();
}

/**
 * Draw the canvas (separated for redrawing with hover effects)
 */
function drawCanvas(hoveredIndex = -1) {
    if (!boardCtx || !window.visualizationParams) return;

    const { minX, minY, margin, scale, offsetX, offsetY, 
            canvasWidth, canvasHeight, scaledBoardWidth, scaledBoardHeight, maxX, maxY } = window.visualizationParams;
    const boardWidth = (maxX - minX) + (2 * margin);
    const boardHeight = (maxY - minY) + (2 * margin);

    // Clear canvas
    boardCtx.clearRect(0, 0, canvasWidth, canvasHeight);

    // Draw background
    boardCtx.fillStyle = '#f5f5f5';
    boardCtx.fillRect(0, 0, canvasWidth, canvasHeight);

    // Draw board rectangle
    boardCtx.fillStyle = '#2d5016';
    boardCtx.strokeStyle = '#1a3010';
    boardCtx.lineWidth = 2;
    boardCtx.fillRect(offsetX, offsetY, scaledBoardWidth, scaledBoardHeight);
    boardCtx.strokeRect(offsetX, offsetY, scaledBoardWidth, scaledBoardHeight);

    // Draw grid lines (optional, for reference)
    boardCtx.strokeStyle = '#3a6820';
    boardCtx.lineWidth = 0.5;
    boardCtx.setLineDash([3, 3]);

    // Vertical grid lines every 10mm
    for (let x = 0; x <= boardWidth; x += 10) {
        const canvasX = offsetX + x * scale;
        boardCtx.beginPath();
        boardCtx.moveTo(canvasX, offsetY);
        boardCtx.lineTo(canvasX, offsetY + scaledBoardHeight);
        boardCtx.stroke();
    }

    // Horizontal grid lines every 10mm
    for (let y = 0; y <= boardHeight; y += 10) {
        const canvasY = offsetY + y * scale;
        boardCtx.beginPath();
        boardCtx.moveTo(offsetX, canvasY);
        boardCtx.lineTo(offsetX + scaledBoardWidth, canvasY);
        boardCtx.stroke();
    }

    boardCtx.setLineDash([]);

    // Draw drill holes (in two passes: holes first, then tooltip on top)
    for (let i = 0; i < drillPoints.length; i++) {
        const point = drillPoints[i];

        // Calculate position on canvas
        const x = offsetX + (point.x - minX + margin) * scale;
        const y = offsetY + (point.y - minY + margin) * scale;

        // Draw hole (circle)
        const holeRadius = Math.max(3, scale * 0.8);  // At least 3px, scales with board

        // Check if hole is excluded
        const isExcluded = excludedHoles.has(i);
        const isHovered = (i === hoveredIndex);

        // Outer circle (copper pad) - red if excluded, gold if hovered, normal otherwise
        if (isExcluded) {
            boardCtx.fillStyle = '#616161ff';
        } else {
            boardCtx.fillStyle = isHovered ? '#ffd700' : '#d4af37';
        }
        boardCtx.beginPath();
        boardCtx.arc(x, y, holeRadius * (isHovered ? 2 : 1.5), 0, Math.PI * 2);
        boardCtx.fill();

        // Inner circle (drill hole)
        boardCtx.fillStyle = '#1a1a1a';
        boardCtx.beginPath();
        boardCtx.arc(x, y, holeRadius, 0, Math.PI * 2);
        boardCtx.fill();

        if (!isExcluded && drillPoints.length <= 50) {
            // Hole number (for small number of holes, only if not excluded)
            boardCtx.fillStyle = '#ffffff';
            boardCtx.font = `${Math.max(8, scale * 1.5)}px Arial`;
            boardCtx.textAlign = 'center';
            boardCtx.textBaseline = 'middle';
            boardCtx.fillText((i + 1).toString(), x, y);
        }
    }

    // Draw tooltip on top of everything (second pass)
    if (hoveredIndex >= 0 && hoveredIndex < drillPoints.length) {
        const point = drillPoints[hoveredIndex];
        const x = offsetX + (point.x - minX + margin) * scale;
        const y = offsetY + (point.y - minY + margin) * scale;
        const holeRadius = Math.max(3, scale * 0.8);
        const isExcluded = excludedHoles.has(hoveredIndex);

        const tooltipText = isExcluded 
            ? `Hole ${hoveredIndex + 1} - EXCLUDED`
            : `Hole ${hoveredIndex + 1}: (${point.x.toFixed(2)}, ${point.y.toFixed(2)}) mm`;
        
        // Measure text width for background
        boardCtx.font = '14px Arial';
        const textMetrics = boardCtx.measureText(tooltipText);
        const tooltipWidth = textMetrics.width + 16;
        const tooltipHeight = 24;
        
        // Position tooltip above the hole
        let tooltipX = x - tooltipWidth / 2;
        let tooltipY = y - holeRadius * 2 - tooltipHeight - 5;
        
        // Keep tooltip within canvas bounds
        if (tooltipX < 5) tooltipX = 5;
        if (tooltipX + tooltipWidth > canvasWidth - 5) tooltipX = canvasWidth - tooltipWidth - 5;
        if (tooltipY < 5) tooltipY = y + holeRadius * 2 + 5;
        
        // Draw tooltip background
        boardCtx.fillStyle = 'rgba(0, 0, 0, 0.85)';
        boardCtx.strokeStyle = isExcluded ? '#cc0000' : '#ffd700';
        boardCtx.lineWidth = 2;
        boardCtx.beginPath();
        boardCtx.roundRect(tooltipX, tooltipY, tooltipWidth, tooltipHeight, 4);
        boardCtx.fill();
        boardCtx.stroke();
        
        // Draw tooltip text
        boardCtx.fillStyle = '#ffffff';
        boardCtx.textAlign = 'left';
        boardCtx.textBaseline = 'middle';
        boardCtx.fillText(tooltipText, tooltipX + 8, tooltipY + tooltipHeight / 2);
    }

    // Draw coordinate axes
    boardCtx.strokeStyle = '#333';
    boardCtx.lineWidth = 1;
    boardCtx.font = '12px Arial';
    boardCtx.fillStyle = '#333';

    // Origin marker
    const originX = offsetX + margin * scale;
    const originY = offsetY + margin * scale;
    boardCtx.beginPath();
    boardCtx.moveTo(originX - 5, originY);
    boardCtx.lineTo(originX + 5, originY);
    boardCtx.moveTo(originX, originY - 5);
    boardCtx.lineTo(originX, originY + 5);
    boardCtx.stroke();
    boardCtx.fillText('(0,0)', originX + 8, originY - 8);

    // Update info displays
    if (drillCountDisplay) {
        const includedCount = drillPoints.length - excludedHoles.size;
        const excludedCount = excludedHoles.size;
        if (excludedCount > 0) {
            drillCountDisplay.textContent = `${includedCount}/${drillPoints.length} holes (${excludedCount} excluded)`;
        } else {
            drillCountDisplay.textContent = `${drillPoints.length} hole${drillPoints.length !== 1 ? 's' : ''}`;
        }
    }
    if (boardDimensionsDisplay) {
        boardDimensionsDisplay.textContent = `Board: ${(maxX - minX).toFixed(1)} × ${(maxY - minY).toFixed(1)} mm`;
    }
}

/**
 * Handle canvas click to toggle hole exclusion
 */
function handleCanvasClick(event) {
    if (!boardCanvas || !drillPoints || !window.visualizationParams) {
        return;
    }

    const rect = boardCanvas.getBoundingClientRect();
    const mouseX = event.clientX - rect.left;
    const mouseY = event.clientY - rect.top;

    const { minX, minY, margin, scale, offsetX, offsetY } = window.visualizationParams;
    const holeRadius = Math.max(3, scale * 0.8);
    const clickDistance = holeRadius * 2;

    // Find clicked hole
    for (let i = 0; i < drillPoints.length; i++) {
        const point = drillPoints[i];
        const x = offsetX + (point.x - minX + margin) * scale;
        const y = offsetY + (point.y - minY + margin) * scale;

        const distance = Math.sqrt((mouseX - x) ** 2 + (mouseY - y) ** 2);
        if (distance <= clickDistance) {
            // Toggle exclusion
            if (excludedHoles.has(i)) {
                excludedHoles.delete(i);
            } else {
                excludedHoles.add(i);
            }
            
            // Redraw canvas with updated exclusions
            drawCanvas(window.lastHoveredIndex);
            
            // Update preview displays in real-time
            updatePreviewDisplays();
            break;
        }
    }
}

/**
 * Handle mouse hover over canvas to show hole coordinates
 */
function handleCanvasHover(event) {
    if (!boardCanvas || !drillPoints || !window.visualizationParams) {
        return;
    }

    const rect = boardCanvas.getBoundingClientRect();
    const mouseX = event.clientX - rect.left;
    const mouseY = event.clientY - rect.top;

    const { minX, minY, margin, scale, offsetX, offsetY } = window.visualizationParams;
    const holeRadius = Math.max(3, scale * 0.8);
    const hoverDistance = holeRadius * 2;  // Detection radius

    let hoveredIndex = -1;

    // Check if mouse is over any hole
    for (let i = 0; i < drillPoints.length; i++) {
        const point = drillPoints[i];
        const x = offsetX + (point.x - minX + margin) * scale;
        const y = offsetY + (point.y - minY + margin) * scale;

        const distance = Math.sqrt((mouseX - x) ** 2 + (mouseY - y) ** 2);
        if (distance <= hoverDistance) {
            hoveredIndex = i;
            boardCanvas.style.cursor = 'pointer';
            break;
        }
    }

    if (hoveredIndex === -1) {
        boardCanvas.style.cursor = 'default';
    }

    // Redraw if hover state changed
    if (hoveredIndex !== window.lastHoveredIndex) {
        window.lastHoveredIndex = hoveredIndex;
        drawCanvas(hoveredIndex);
    }
}
