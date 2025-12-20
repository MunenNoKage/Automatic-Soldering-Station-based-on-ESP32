/**
 * @file manual.js
 * @brief Manual control page logic
 *
 * Handles manual positioning and control of the soldering station.
 */

// DOM elements
let manualEnterBtn;
let manualMoveBtn;
let manualSetOriginBtn;
let manualExitBtn;
let manualXInput;
let manualYInput;
let manualZInput;
let manualStatus;
let solderFeedBtn;
let solderAmountInput;
let solderStatus;

// Canvas elements
let boardCanvas = null;
let boardCtx = null;
let currentPositionDisplay = null;
let boardDimensionsDisplay = null;

// Visualization data
let currentPosition = { x: 0, y: 0, z: 0 };
let targetPosition = null;  // null until a move command is issued
let positionUpdateInterval = null;

// Coordinate limits (from hardware configuration)
let coordinateLimits = {
    x: { min: 0, max: 250 },
    y: { min: 0, max: 210 },
    z: { min: 0, max: 180 }
};

/**
 * Initialize the manual control page
 */
document.addEventListener('DOMContentLoaded', function() {
    // Get DOM elements
    manualEnterBtn = document.getElementById('manual-enter-btn');
    manualMoveBtn = document.getElementById('manual-move-btn');
    manualSetOriginBtn = document.getElementById('manual-set-origin-btn');
    manualExitBtn = document.getElementById('manual-exit-btn');
    manualXInput = document.getElementById('manual-x');
    manualYInput = document.getElementById('manual-y');
    manualZInput = document.getElementById('manual-z');
    manualStatus = document.getElementById('manual-status');

    // Solder control elements
    solderFeedBtn = document.getElementById('solder-feed-btn');
    solderAmountInput = document.getElementById('solder-amount');
    solderStatus = document.getElementById('manual-status');

    // Canvas elements
    boardCanvas = document.getElementById('board-canvas');
    if (boardCanvas) {
        boardCtx = boardCanvas.getContext('2d');
        // Add click event listener for canvas
        boardCanvas.addEventListener('click', handleCanvasClick);
    }
    currentPositionDisplay = document.getElementById('current-position');
    boardDimensionsDisplay = document.getElementById('board-dimensions');

    // Add event listeners
    if (manualEnterBtn) {
        manualEnterBtn.addEventListener('click', handleManualEnter);
    }

    if (manualMoveBtn) {
        manualMoveBtn.addEventListener('click', handleManualMove);
    }

    if (manualSetOriginBtn) {
        manualSetOriginBtn.addEventListener('click', handleSetOrigin);
    }

    if (manualExitBtn) {
        manualExitBtn.addEventListener('click', handleManualExit);
    }

    if (solderFeedBtn) {
        solderFeedBtn.addEventListener('click', handleSolderFeed);
    }

    // Set input field min/max attributes based on hardcoded limits
    if (manualXInput) {
        manualXInput.min = coordinateLimits.x.min;
        manualXInput.max = coordinateLimits.x.max;
        manualXInput.addEventListener('input', handleCoordinateInput);
    }
    if (manualYInput) {
        manualYInput.min = coordinateLimits.y.min;
        manualYInput.max = coordinateLimits.y.max;
        manualYInput.addEventListener('input', handleCoordinateInput);
    }
    if (manualZInput) {
        manualZInput.min = coordinateLimits.z.min;
        manualZInput.max = coordinateLimits.z.max;
        manualZInput.addEventListener('input', handleCoordinateInput);
    }

    // Initialize visualization
    drawBoard();

    // Start position polling
    startPositionPolling();
});

/**
 * Handle coordinate input changes to update target visualization
 */
function handleCoordinateInput() {
    const x = parseFloat(manualXInput.value);
    const y = parseFloat(manualYInput.value);
    const z = parseFloat(manualZInput.value);

    // Only update target if all coordinates are valid numbers
    if (!isNaN(x) && !isNaN(y) && !isNaN(z)) {
        // Clamp to coordinate limits
        const clampedX = Math.max(coordinateLimits.x.min, Math.min(coordinateLimits.x.max, x));
        const clampedY = Math.max(coordinateLimits.y.min, Math.min(coordinateLimits.y.max, y));
        const clampedZ = Math.max(coordinateLimits.z.min, Math.min(coordinateLimits.z.max, z));

        // Update target position for visualization
        targetPosition = { x: clampedX, y: clampedY, z: clampedZ };

        // Redraw visualization to show target
        updateVisualization();
    }
}

/**
 * Handle canvas click to set target coordinates
 */
function handleCanvasClick(event) {
    if (!boardCanvas) return;

    const rect = boardCanvas.getBoundingClientRect();
    
    // Get click position relative to canvas element
    const clickX = event.clientX - rect.left;
    const clickY = event.clientY - rect.top;
    
    // Scale to canvas internal coordinates (canvas might be scaled via CSS)
    const scaleX = boardCanvas.width / rect.width;
    const scaleY = boardCanvas.height / rect.height;
    const canvasX = clickX * scaleX;
    const canvasY = clickY * scaleY;

    // Convert canvas coordinates to world coordinates
    const worldCoords = machineCanvasToWorld(boardCanvas, canvasX, canvasY, coordinateLimits);

    // Clamp to coordinate limits
    const x = Math.max(coordinateLimits.x.min, Math.min(coordinateLimits.x.max, worldCoords.x));
    const y = Math.max(coordinateLimits.y.min, Math.min(coordinateLimits.y.max, worldCoords.y));

    // Update input fields
    if (manualXInput) manualXInput.value = x.toFixed(1);
    if (manualYInput) manualYInput.value = y.toFixed(1);

    // Update target position for visualization
    targetPosition = { 
        x: x, 
        y: y, 
        z: manualZInput ? parseFloat(manualZInput.value) : currentPosition.z 
    };

    // Redraw visualization to show target
    updateVisualization();
}

/**
 * Handle manual control mode entry
 */
async function handleManualEnter() {
    manualStatus.textContent = 'Entering manual control mode...';
    manualStatus.className = 'upload-status';

    try {
        const response = await fetch('/api/manual/enter', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            }
        });

        const result = await response.json();

        if (result.success) {
            manualStatus.textContent = 'Manual control mode activated';
            manualStatus.className = 'upload-status success';

            // Enable move, set origin, and exit buttons
            if (manualEnterBtn) manualEnterBtn.disabled = true;
            if (manualMoveBtn) manualMoveBtn.disabled = false;
            if (manualSetOriginBtn) manualSetOriginBtn.disabled = false;
            if (manualExitBtn) manualExitBtn.disabled = false;
            if (solderFeedBtn) solderFeedBtn.disabled = false;

        } else {
            manualStatus.textContent = 'Error: ' + (result.message || 'Failed to enter manual mode');
            manualStatus.className = 'upload-status error';
        }
    } catch (error) {
        manualStatus.textContent = 'Error: ' + error.message;
        manualStatus.className = 'upload-status error';
    }
}

/**
 * Handle manual move command
 */
async function handleManualMove() {
    const x = parseFloat(manualXInput.value);
    const y = parseFloat(manualYInput.value);
    const z = parseFloat(manualZInput.value);

    if (isNaN(x) || isNaN(y) || isNaN(z)) {
        manualStatus.textContent = 'Error: Invalid coordinates';
        manualStatus.className = 'upload-status error';
        return;
    }

    // Validate coordinate ranges
    if (x < coordinateLimits.x.min || x > coordinateLimits.x.max) {
        manualStatus.textContent = `Error: X coordinate must be between ${coordinateLimits.x.min} and ${coordinateLimits.x.max} mm`;
        manualStatus.className = 'upload-status error';
        return;
    }
    if (y < coordinateLimits.y.min || y > coordinateLimits.y.max) {
        manualStatus.textContent = `Error: Y coordinate must be between ${coordinateLimits.y.min} and ${coordinateLimits.y.max} mm`;
        manualStatus.className = 'upload-status error';
        return;
    }
    if (z < coordinateLimits.z.min || z > coordinateLimits.z.max) {
        manualStatus.textContent = `Error: Z coordinate must be between ${coordinateLimits.z.min} and ${coordinateLimits.z.max} mm`;
        manualStatus.className = 'upload-status error';
        return;
    }

    // Update target position for visualization
    targetPosition = { x, y, z };

    manualStatus.textContent = `Moving to X=${x.toFixed(2)}, Y=${y.toFixed(2)}, Z=${z.toFixed(2)}...`;
    manualStatus.className = 'upload-status';

    try {
        const response = await fetch('/api/manual/move', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ x: x, y: y, z: z })
        });

        const result = await response.json();

        if (result.success) {
            manualStatus.textContent = `Command sent: Move to X=${x.toFixed(2)}, Y=${y.toFixed(2)}, Z=${z.toFixed(2)}`;
            manualStatus.className = 'upload-status success';
        } else {
            manualStatus.textContent = 'Error: ' + (result.message || 'Failed to send move command');
            manualStatus.className = 'upload-status error';
        }
    } catch (error) {
        manualStatus.textContent = 'Error: ' + error.message;
        manualStatus.className = 'upload-status error';
    }
}

/**
 * Handle set origin command
 */
async function handleSetOrigin() {
    manualStatus.textContent = 'Setting origin coordinates to current position...';
    manualStatus.className = 'upload-status';

    try {
        const response = await fetch('/api/manual/set_origin', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            }
        });

        const result = await response.json();

        if (result.success) {
            manualStatus.textContent = `Origin set to X=${result.x_origin.toFixed(2)} mm, Y=${result.y_origin.toFixed(2)} mm`;
            manualStatus.className = 'upload-status success';
        } else {
            manualStatus.textContent = 'Error: ' + (result.message || 'Failed to set origin');
            manualStatus.className = 'upload-status error';
        }
    } catch (error) {
        manualStatus.textContent = 'Error: ' + error.message;
        manualStatus.className = 'upload-status error';
    }
}

/**
 * Handle manual control mode exit
 */
async function handleManualExit() {
    manualStatus.textContent = 'Exiting manual control mode...';
    manualStatus.className = 'upload-status';

    try {
        const response = await fetch('/api/manual/exit', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            }
        });

        const result = await response.json();

        if (result.success) {
            manualStatus.textContent = 'Exited manual control mode';
            manualStatus.className = 'upload-status success';

            // Reset buttons
            if (manualEnterBtn) manualEnterBtn.disabled = false;
            if (manualMoveBtn) manualMoveBtn.disabled = true;
            if (manualSetOriginBtn) manualSetOriginBtn.disabled = true;
            if (manualExitBtn) manualExitBtn.disabled = true;
        } else {
            manualStatus.textContent = 'Error: ' + (result.message || 'Failed to exit manual mode');
            manualStatus.className = 'upload-status error';
        }
    } catch (error) {
        manualStatus.textContent = 'Error: ' + error.message;
        manualStatus.className = 'upload-status error';
    }
}

/**
 * Handle solder feed command
 */
async function handleSolderFeed() {
    const amount = parseInt(solderAmountInput.value);

    if (isNaN(amount) || amount < -1000 || amount > 1000) {
        solderStatus.textContent = 'Error: Feed amount must be between -1000 and 1000 steps';
        solderStatus.className = 'upload-status error';
        return;
    }

    solderStatus.textContent = `Feeding ${amount} steps of solder...`;
    solderStatus.className = 'upload-status';

    try {
        const response = await fetch('/api/manual/solder', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ amount: amount })
        });

        const result = await response.json();

        if (result.success) {
            solderStatus.textContent = `Solder fed: ${amount} steps`;
            solderStatus.className = 'upload-status success';
            setTimeout(() => {
                solderStatus.textContent = '';
            }, 3000);
        } else {
            solderStatus.textContent = 'Error: ' + (result.message || 'Failed to feed solder');
            solderStatus.className = 'upload-status error';
        }
    } catch (error) {
        solderStatus.textContent = 'Error: ' + error.message;
        solderStatus.className = 'upload-status error';
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

                // Check if we've reached the target position
                if (targetPosition !== null) {
                    const dx = Math.abs(position.x - targetPosition.x);
                    const dy = Math.abs(position.y - targetPosition.y);
                    const dz = Math.abs(position.z - targetPosition.z);
                    
                    // If within 0.5mm of target on all axes, clear target
                    if (dx < 0.5 && dy < 0.5 && dz < 0.5) {
                        targetPosition = null;
                    }
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
 * Draw the board with working area dimensions
 */
function drawBoard() {
    if (!boardCtx) return;

    if (boardDimensionsDisplay) {
        const width = coordinateLimits.x.max - coordinateLimits.x.min;
        const height = coordinateLimits.y.max - coordinateLimits.y.min;
        boardDimensionsDisplay.textContent = `Working Area: ${width}mm × ${height}mm`;
    }

    updateVisualization();
}

/**
 * Update visualization with current and target positions
 */
function updateVisualization() {
    if (!boardCtx) return;

    // Use machine_canvas module to draw everything
    // Pass currentPosition as target if no target is set (distance will be 0, no path drawn)
    drawMachineCanvas(boardCtx, coordinateLimits, currentPosition, targetPosition || currentPosition);
}

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    stopPositionPolling();
});
