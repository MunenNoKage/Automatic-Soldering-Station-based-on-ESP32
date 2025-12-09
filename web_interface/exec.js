/**
 * @file exec.js
 * @brief Execution control page logic
 *
 * Handles G-Code execution control (start, pause, resume, stop).
 */

// DOM elements
let startBtn;
let pauseBtn;
let resumeBtn;
let stopBtn;
let controlStatus;

// Canvas elements
let boardCanvas = null;
let boardCtx = null;
let currentPositionDisplay = null;
let boardDimensionsDisplay = null;

// Visualization data
let drillPoints = [];
let currentPosition = { x: 0, y: 0, z: 0 };
let positionUpdateInterval = null;

/**
 * Initialize the execution control page
 */
document.addEventListener('DOMContentLoaded', function() {
    // Get DOM elements
    startBtn = document.getElementById('start-btn');
    pauseBtn = document.getElementById('pause-btn');
    resumeBtn = document.getElementById('resume-btn');
    stopBtn = document.getElementById('stop-btn');
    controlStatus = document.getElementById('control-status');

    // Canvas elements
    boardCanvas = document.getElementById('board-canvas');
    if (boardCanvas) {
        boardCtx = boardCanvas.getContext('2d');
    }
    currentPositionDisplay = document.getElementById('current-position');
    boardDimensionsDisplay = document.getElementById('board-dimensions');

    // Add event listeners
    if (startBtn) {
        startBtn.addEventListener('click', handleStart);
    }

    if (pauseBtn) {
        pauseBtn.addEventListener('click', handlePause);
    }

    if (resumeBtn) {
        resumeBtn.addEventListener('click', handleResume);
    }

    if (stopBtn) {
        stopBtn.addEventListener('click', handleStop);
    }

    // Load G-code data and initialize visualization
    loadGCodeData();
    
    // Start position polling
    startPositionPolling();
});

/**
 * Handle start button click
 */
async function handleStart() {
    controlStatus.textContent = 'Starting execution...';
    controlStatus.className = 'upload-status info';

    try {
        const response = await fetch('/api/gcode/start', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            }
        });

        if (response.ok) {
            const result = await response.json();
            controlStatus.textContent = 'Execution started';
            controlStatus.className = 'upload-status success';
            
            // Update button states
            startBtn.disabled = true;
            pauseBtn.disabled = false;
            resumeBtn.disabled = true;
            stopBtn.disabled = false;

        } else {
            const error = await response.json();
            controlStatus.textContent = `Error: ${error.message || 'Failed to start'}`;
            controlStatus.className = 'upload-status error';
        }
    } catch (error) {
        controlStatus.textContent = `Error: ${error.message}`;
        controlStatus.className = 'upload-status error';
    }
}

/**
 * Handle pause button click
 */
async function handlePause() {
    controlStatus.textContent = 'Pausing execution...';
    controlStatus.className = 'upload-status info';

    try {
        const response = await fetch('/api/gcode/pause', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            }
        });

        if (response.ok) {
            const result = await response.json();
            controlStatus.textContent = 'Execution paused';
            controlStatus.className = 'upload-status success';

            // Update button states
            pauseBtn.disabled = true;
            resumeBtn.disabled = false;
        } else {
            const error = await response.json();
            controlStatus.textContent = `Error: ${error.message || 'Failed to pause'}`;
            controlStatus.className = 'upload-status error';
        }
    } catch (error) {
        controlStatus.textContent = `Error: ${error.message}`;
        controlStatus.className = 'upload-status error';
    }
}

/**
 * Handle resume button click
 */
async function handleResume() {
    controlStatus.textContent = 'Resuming execution...';
    controlStatus.className = 'upload-status info';

    try {
        const response = await fetch('/api/gcode/resume', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            }
        });

        if (response.ok) {
            const result = await response.json();
            controlStatus.textContent = 'Execution resumed';
            controlStatus.className = 'upload-status success';

            // Update button states
            pauseBtn.disabled = false;
            resumeBtn.disabled = true;
        } else {
            const error = await response.json();
            controlStatus.textContent = `Error: ${error.message || 'Failed to resume'}`;
            controlStatus.className = 'upload-status error';
        }
    } catch (error) {
        controlStatus.textContent = `Error: ${error.message}`;
        controlStatus.className = 'upload-status error';
    }
}

/**
 * Handle stop button click
 */
async function handleStop() {
    controlStatus.textContent = 'Stopping execution...';
    controlStatus.className = 'upload-status info';

    try {
        const response = await fetch('/api/gcode/stop', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            }
        });

        if (response.ok) {
            const result = await response.json();
            controlStatus.textContent = 'Execution stopped';
            controlStatus.className = 'upload-status success';

            // Reset button states
            startBtn.disabled = false;
            pauseBtn.disabled = true;
            resumeBtn.disabled = true;
            stopBtn.disabled = true;

            setTimeout(() => {
                controlStatus.textContent = '';
            }, 3000);
        } else {
            const error = await response.json();
            controlStatus.textContent = `Error: ${error.message || 'Failed to stop'}`;
            controlStatus.className = 'upload-status error';
        }
    } catch (error) {
        controlStatus.textContent = `Error: ${error.message}`;
        controlStatus.className = 'upload-status error';
    }
}

/**
 * Load G-code data from session storage
 */
function loadGCodeData() {
    const gcode = sessionStorage.getItem('uploadedGCode');
    if (!gcode) {
        console.warn('No G-code data found in session storage');
        return;
    }

    // Parse G-code to extract drill points
    drillPoints = [];
    const lines = gcode.split('\n');
    
    for (const line of lines) {
        const trimmed = line.trim();
        if (trimmed.startsWith('G0') || trimmed.startsWith('G1')) {
            let x = null, y = null;
            
            const xMatch = trimmed.match(/X([-]?\d+\.?\d*)/);
            const yMatch = trimmed.match(/Y([-]?\d+\.?\d*)/);
            
            if (xMatch) x = parseFloat(xMatch[1]);
            if (yMatch) y = parseFloat(yMatch[1]);
            
            if (x !== null && y !== null) {
                drillPoints.push({ x, y });
            }
        }
    }

    console.log(`Loaded ${drillPoints.length} drill points from G-code`);
    
    if (drillPoints.length > 0 && boardCanvas) {
        drawBoard();
    }
}

/**
 * Start polling for position updates
 */
function startPositionPolling() {
    if (positionUpdateInterval) {
        clearInterval(positionUpdateInterval);
    }

    positionUpdateInterval = setInterval(async () => {
        try {
            const response = await fetch('/api/status/position');
            if (response.ok) {
                const position = await response.json();
                currentPosition = position;
                
                if (currentPositionDisplay) {
                    currentPositionDisplay.textContent = `X: ${position.x.toFixed(2)}mm, Y: ${position.y.toFixed(2)}mm, Z: ${position.z.toFixed(2)}mm`;
                }
                
                if (boardCanvas) {
                    updateVisualization();
                }
            }
        } catch (error) {
            console.error('Error fetching position:', error);
        }
    }, 500);
}

/**
 * Stop position polling
 */
function stopPositionPolling() {
    if (positionUpdateInterval) {
        clearInterval(positionUpdateInterval);
        positionUpdateInterval = null;
    }
}

/**
 * Draw the board with drill holes
 */
function drawBoard() {
    if (!boardCtx || drillPoints.length === 0) return;

    // Calculate board bounds
    let minX = Infinity, minY = Infinity;
    let maxX = -Infinity, maxY = -Infinity;

    for (const point of drillPoints) {
        minX = Math.min(minX, point.x);
        minY = Math.min(minY, point.y);
        maxX = Math.max(maxX, point.x);
        maxY = Math.max(maxY, point.y);
    }

    // Add margin around the board (10mm on each side)
    const margin = 10;
    const boardWidth = (maxX - minX) + (2 * margin);
    const boardHeight = (maxY - minY) + (2 * margin);

    // Store visualization parameters globally
    window.execVisualizationParams = {
        minX, minY, maxX, maxY,
        margin,
        boardWidth, boardHeight
    };

    if (boardDimensionsDisplay) {
        boardDimensionsDisplay.textContent = `${(maxX - minX).toFixed(1)}mm × ${(maxY - minY).toFixed(1)}mm`;
    }

    updateVisualization();
}

/**
 * Update visualization with current position
 */
function updateVisualization() {
    if (!boardCtx || !window.execVisualizationParams) return;

    const canvas = boardCanvas;
    const ctx = boardCtx;
    const params = window.execVisualizationParams;

    // Clear canvas
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // Add padding
    const padding = 40;
    const drawWidth = canvas.width - 2 * padding;
    const drawHeight = canvas.height - 2 * padding;

    // Calculate scale
    const scaleX = drawWidth / params.boardWidth;
    const scaleY = drawHeight / params.boardHeight;
    const scale = Math.min(scaleX, scaleY);

    // Center the board
    const boardPixelWidth = params.boardWidth * scale;
    const boardPixelHeight = params.boardHeight * scale;
    const offsetX = padding + (drawWidth - boardPixelWidth) / 2;
    const offsetY = padding + (drawHeight - boardPixelHeight) / 2;

    // Helper function to convert world coordinates to canvas coordinates
    const worldToCanvas = (worldX, worldY) => {
        const canvasX = offsetX + (worldX - params.minX + params.margin) * scale;
        const canvasY = offsetY + (worldY - params.minY + params.margin) * scale;
        return { x: canvasX, y: canvasY };
    };

    // Draw PCB background
    ctx.fillStyle = '#2d5016';
    ctx.fillRect(offsetX, offsetY, boardPixelWidth, boardPixelHeight);

    // Draw grid
    ctx.strokeStyle = '#3d6020';
    ctx.lineWidth = 0.5;
    ctx.setLineDash([3, 3]);
    const gridSpacing = 10; // 10mm grid

    // Vertical grid lines
    for (let x = 0; x <= params.boardWidth; x += gridSpacing) {
        const canvasX = offsetX + x * scale;
        ctx.beginPath();
        ctx.moveTo(canvasX, offsetY);
        ctx.lineTo(canvasX, offsetY + boardPixelHeight);
        ctx.stroke();
    }

    // Horizontal grid lines
    for (let y = 0; y <= params.boardHeight; y += gridSpacing) {
        const canvasY = offsetY + y * scale;
        ctx.beginPath();
        ctx.moveTo(offsetX, canvasY);
        ctx.lineTo(offsetX + boardPixelWidth, canvasY);
        ctx.stroke();
    }

    ctx.setLineDash([]);

    // Draw origin marker
    const origin = worldToCanvas(0, 0);
    if (origin.x >= offsetX && origin.x <= offsetX + boardPixelWidth &&
        origin.y >= offsetY && origin.y <= offsetY + boardPixelHeight) {
        ctx.strokeStyle = '#ffffff';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(origin.x - 10, origin.y);
        ctx.lineTo(origin.x + 10, origin.y);
        ctx.moveTo(origin.x, origin.y - 10);
        ctx.lineTo(origin.x, origin.y + 10);
        ctx.stroke();

        ctx.fillStyle = '#ffffff';
        ctx.font = '12px monospace';
        ctx.fillText('(0,0)', origin.x + 12, origin.y - 5);
    }

    // Draw drill holes
    for (let i = 0; i < drillPoints.length; i++) {
        const point = drillPoints[i];
        const pos = worldToCanvas(point.x, point.y);

        // Gold pad
        ctx.fillStyle = '#d4af37';
        ctx.beginPath();
        ctx.arc(pos.x, pos.y, 8, 0, 2 * Math.PI);
        ctx.fill();

        // Black center
        ctx.fillStyle = '#000000';
        ctx.beginPath();
        ctx.arc(pos.x, pos.y, 3, 0, 2 * Math.PI);
        ctx.fill();

        // Hole number
        ctx.fillStyle = '#ffffff';
        ctx.font = '10px monospace';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText((i + 1).toString(), pos.x, pos.y);
    }

    // Draw current position marker
    const currentPos = worldToCanvas(currentPosition.x, currentPosition.y);
    
    // Red crosshair
    ctx.strokeStyle = '#ff0000';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(currentPos.x - 10, currentPos.y);
    ctx.lineTo(currentPos.x + 10, currentPos.y);
    ctx.moveTo(currentPos.x, currentPos.y - 10);
    ctx.lineTo(currentPos.x, currentPos.y + 10);
    ctx.stroke();

    // Red circle
    ctx.strokeStyle = '#ff0000';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.arc(currentPos.x, currentPos.y, 8, 0, 2 * Math.PI);
    ctx.stroke();
}

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    stopPositionPolling();
});
