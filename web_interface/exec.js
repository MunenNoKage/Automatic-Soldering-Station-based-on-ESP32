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

    // Calculate visualization parameters using board_canvas module
    const params = calculateBoardCanvasParams(boardCanvas, drillPoints, 0);

    // Store visualization parameters globally
    window.execVisualizationParams = params;

    if (boardDimensionsDisplay) {
        boardDimensionsDisplay.textContent = `${(params.maxX - params.minX).toFixed(1)}mm × ${(params.maxY - params.minY).toFixed(1)}mm`;
    }

    updateVisualization();
}

/**
 * Update visualization with current position
 */
function updateVisualization() {
    if (!boardCtx || !window.execVisualizationParams) return;

    // Use board_canvas module to draw everything
    drawBoardCanvas(
        boardCtx,
        window.execVisualizationParams,
        drillPoints,
        currentPosition,
        null, // no excluded holes in exec mode
        -1    // no hovered index in exec mode
    );
}

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    stopPositionPolling();
});
