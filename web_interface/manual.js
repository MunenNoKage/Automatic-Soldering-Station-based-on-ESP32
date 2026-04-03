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
let solderTakeBtn;
let solderAmountInput;
let solderStatus;
let heatingEnableBtn;
let heatingDisableBtn;
let heatingStatus;
let currentTempDisplay;
let targetTempDisplay;

// Canvas elements
let boardCanvas = null;
let boardCtx = null;
let currentPositionDisplay = null;
let boardDimensionsDisplay = null;

// Visualization data
let currentPosition = { x: 0, y: 0, z: 0 };
let targetPosition = null;  // null until a move command is issued
let positionUpdateInterval = null;
let heatingEnabled = false;
let manualModeActive = false;

// Coordinate limits (from hardware configuration)
let coordinateLimits = {
    x: { min: 0, max: 250 },
    y: { min: 0, max: 210 },
    z: { min: 0, max: 210 }
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
    solderTakeBtn = document.getElementById('solder-take-btn');
    solderAmountInput = document.getElementById('solder-amount');
    solderStatus = document.getElementById('manual-status');

    // Heating control elements
    heatingEnableBtn = document.getElementById('heating-enable-btn');
    heatingDisableBtn = document.getElementById('heating-disable-btn');
    heatingStatus = document.getElementById('heating-status');
    currentTempDisplay = document.getElementById('current-temp');
    targetTempDisplay = document.getElementById('target-temp');

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

    if (solderTakeBtn) {
        solderTakeBtn.addEventListener('click', handleSolderTake);
    }

    if (heatingEnableBtn) {
        heatingEnableBtn.addEventListener('click', handleEnableHeating);
    }

    if (heatingDisableBtn) {
        heatingDisableBtn.addEventListener('click', handleDisableHeating);
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
            manualModeActive = true;

            // Enable move, set origin, and exit buttons
            if (manualEnterBtn) manualEnterBtn.disabled = true;
            if (manualMoveBtn) manualMoveBtn.disabled = false;
            if (manualSetOriginBtn) manualSetOriginBtn.disabled = false;
            if (manualExitBtn) manualExitBtn.disabled = false;
            if (solderFeedBtn) solderFeedBtn.disabled = false;
            if (solderTakeBtn) solderTakeBtn.disabled = false;

            // Enable heating controls
            if (heatingEnableBtn) heatingEnableBtn.disabled = false;
            if (heatingDisableBtn) heatingDisableBtn.disabled = true;

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
            manualModeActive = false;

            // Reset buttons
            if (manualEnterBtn) manualEnterBtn.disabled = false;
            if (manualMoveBtn) manualMoveBtn.disabled = true;
            if (manualSetOriginBtn) manualSetOriginBtn.disabled = true;
            if (manualExitBtn) manualExitBtn.disabled = true;
            if (solderFeedBtn) solderFeedBtn.disabled = true;
            if (solderTakeBtn) solderTakeBtn.disabled = true;

            // Disable heating controls
            if (heatingEnableBtn) heatingEnableBtn.disabled = true;
            if (heatingDisableBtn) heatingDisableBtn.disabled = true;
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

    if (isNaN(amount) || amount < 0 || amount > 1000) {
        solderStatus.textContent = 'Error: Feed amount must be between 0 and 1000 mm';
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
            solderStatus.textContent = `Solder fed: ${amount} mm`;
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

async function handleSolderTake() {
    const amount = parseInt(solderAmountInput.value);

    if (isNaN(amount) || amount < 0 || amount > 1000) {
        solderStatus.textContent = 'Error: Feed amount must be between 0 and 1000 mm';
        solderStatus.className = 'upload-status error';
        return;
    }

    solderStatus.textContent = `Taking ${amount} steps of solder...`;
    solderStatus.className = 'upload-status';

    try {
        const response = await fetch('/api/manual/solder', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ amount: -amount })
        });

        const result = await response.json();

        if (result.success) {
            solderStatus.textContent = `Solder fed: ${amount} mm`;
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
                const data = await response.json();
                currentPosition = { x: data.x || 0, y: data.y || 0, z: data.z || 0 };

                if (currentPositionDisplay) {
                    currentPositionDisplay.textContent = `X: ${currentPosition.x.toFixed(2)}mm, Y: ${currentPosition.y.toFixed(2)}mm, Z: ${currentPosition.z.toFixed(2)}mm`;
                }

                // Update temperature display if available
                if (data.temperature !== undefined && currentTempDisplay) {
                    currentTempDisplay.textContent = `${data.temperature.toFixed(1)}°C`;
                }

                // Check if we've reached the target position
                if (targetPosition !== null) {
                    const dx = Math.abs(currentPosition.x - targetPosition.x);
                    const dy = Math.abs(currentPosition.y - targetPosition.y);
                    const dz = Math.abs(currentPosition.z - targetPosition.z);

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
    }, 1000);
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

/**
 * Handle enable heating button click
 */
async function handleEnableHeating() {
    if (!manualModeActive) {
        heatingStatus.textContent = 'Error: Must be in manual control mode';
        heatingStatus.className = 'upload-status error';
        return;
    }

    // Disable button immediately to prevent double clicks
    if (heatingEnableBtn) heatingEnableBtn.disabled = true;

    heatingStatus.textContent = 'Enabling heating...';
    heatingStatus.className = 'upload-status info';

    try {
        const response = await fetch('/api/heating/enable', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            }
        });

        if (response.ok) {
            const result = await response.json();
            heatingStatus.textContent = 'Heating enabled';
            heatingStatus.className = 'upload-status success';

            // Update button states
            heatingEnabled = true;
            if (heatingDisableBtn) heatingDisableBtn.disabled = false;

            setTimeout(() => {
                heatingStatus.textContent = '';
            }, 3000);
        } else {
            const error = await response.json();
            heatingStatus.textContent = `Error: ${error.message || 'Failed to enable heating'}`;
            heatingStatus.className = 'upload-status error';
            // Re-enable button on error
            if (heatingEnableBtn) heatingEnableBtn.disabled = false;
        }
    } catch (error) {
        heatingStatus.textContent = `Error: ${error.message}`;
        heatingStatus.className = 'upload-status error';
        // Re-enable button on error
        if (heatingEnableBtn) heatingEnableBtn.disabled = false;
    }
}

/**
 * Handle disable heating button click
 */
async function handleDisableHeating() {
    if (!manualModeActive) {
        heatingStatus.textContent = 'Error: Must be in manual control mode';
        heatingStatus.className = 'upload-status error';
        return;
    }

    // Disable button immediately to prevent double clicks
    if (heatingDisableBtn) heatingDisableBtn.disabled = true;

    heatingStatus.textContent = 'Disabling heating...';
    heatingStatus.className = 'upload-status info';

    try {
        const response = await fetch('/api/heating/disable', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            }
        });

        if (response.ok) {
            const result = await response.json();
            heatingStatus.textContent = 'Heating disabled';
            heatingStatus.className = 'upload-status success';

            // Update button states
            heatingEnabled = false;
            if (heatingEnableBtn) heatingEnableBtn.disabled = false;

            setTimeout(() => {
                heatingStatus.textContent = '';
            }, 3000);
        } else {
            const error = await response.json();
            heatingStatus.textContent = `Error: ${error.message || 'Failed to disable heating'}`;
            heatingStatus.className = 'upload-status error';
            // Re-enable button on error
            if (heatingDisableBtn) heatingDisableBtn.disabled = false;
        }
    } catch (error) {
        heatingStatus.textContent = `Error: ${error.message}`;
        heatingStatus.className = 'upload-status error';
        // Re-enable button on error
        if (heatingDisableBtn) heatingDisableBtn.disabled = false;
    }
}

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    stopPositionPolling();
});
