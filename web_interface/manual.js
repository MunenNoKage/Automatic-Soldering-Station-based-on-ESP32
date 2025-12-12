/**
 * @file manual.js
 * @brief Manual control page logic
 *
 * Handles manual positioning and control of the soldering station.
 */

// DOM elements
let manualEnterBtn;
let manualMoveBtn;
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
let targetPosition = { x: 0, y: 0, z: 0 };
let positionUpdateInterval = null;

// Coordinate limits (fetched from backend)
let coordinateLimits = {
    x: { min: 0, max: 250 },
    y: { min: 0, max: 210 },
    z: { min: 0, max: 180 }
};

// Board dimensions (configurable based on your setup)
const BOARD_WIDTH = 200;  // mm
const BOARD_HEIGHT = 150; // mm

/**
 * Initialize the manual control page
 */
document.addEventListener('DOMContentLoaded', function() {
    // Get DOM elements
    manualEnterBtn = document.getElementById('manual-enter-btn');
    manualMoveBtn = document.getElementById('manual-move-btn');
    manualExitBtn = document.getElementById('manual-exit-btn');
    manualXInput = document.getElementById('manual-x');
    manualYInput = document.getElementById('manual-y');
    manualZInput = document.getElementById('manual-z');
    manualStatus = document.getElementById('manual-status');

    // Solder control elements
    solderFeedBtn = document.getElementById('solder-feed-btn');
    solderAmountInput = document.getElementById('solder-amount');
    solderStatus = document.getElementById('solder-status');

    // Canvas elements
    boardCanvas = document.getElementById('board-canvas');
    if (boardCanvas) {
        boardCtx = boardCanvas.getContext('2d');
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
    }
    if (manualYInput) {
        manualYInput.min = coordinateLimits.y.min;
        manualYInput.max = coordinateLimits.y.max;
    }
    if (manualZInput) {
        manualZInput.min = coordinateLimits.z.min;
        manualZInput.max = coordinateLimits.z.max;
    }

    // Initialize visualization
    drawBoard();
    
    // Start position polling
    startPositionPolling();
});

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
            
            // Enable move and exit buttons
            manualEnterBtn.disabled = true;
            manualMoveBtn.disabled = false;
            manualExitBtn.disabled = false;
            solderFeedBtn.disabled = false;
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
            manualEnterBtn.disabled = false;
            manualMoveBtn.disabled = true;
            manualExitBtn.disabled = true;
            solderFeedBtn.disabled = true;
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
 * Draw the board with fixed dimensions
 */
function drawBoard() {
    if (!boardCtx) return;

    // Store visualization parameters globally
    window.manualVisualizationParams = {
        minX: 0,
        minY: 0,
        maxX: BOARD_WIDTH,
        maxY: BOARD_HEIGHT,
        boardWidth: BOARD_WIDTH,
        boardHeight: BOARD_HEIGHT
    };

    if (boardDimensionsDisplay) {
        boardDimensionsDisplay.textContent = `${BOARD_WIDTH}mm × ${BOARD_HEIGHT}mm`;
    }

    updateVisualization();
}

/**
 * Update visualization with current and target positions
 */
function updateVisualization() {
    if (!boardCtx || !window.manualVisualizationParams) return;

    const canvas = boardCanvas;
    const ctx = boardCtx;
    const params = window.manualVisualizationParams;

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
        const canvasX = offsetX + (worldX - params.minX) * scale;
        const canvasY = offsetY + (worldY - params.minY) * scale;
        return { x: canvasX, y: canvasY };
    };

    // Draw PCB background
    ctx.fillStyle = '#2d5016';
    ctx.fillRect(offsetX, offsetY, boardPixelWidth, boardPixelHeight);

    // Draw grid
    ctx.strokeStyle = '#3d6020';
    ctx.lineWidth = 0.5;
    const gridSpacing = 10; // 10mm grid

    for (let x = Math.ceil(params.minX / gridSpacing) * gridSpacing; x <= params.maxX; x += gridSpacing) {
        const canvasPos = worldToCanvas(x, params.minY);
        ctx.beginPath();
        ctx.moveTo(canvasPos.x, offsetY);
        ctx.lineTo(canvasPos.x, offsetY + boardPixelHeight);
        ctx.stroke();
    }

    for (let y = Math.ceil(params.minY / gridSpacing) * gridSpacing; y <= params.maxY; y += gridSpacing) {
        const canvasPos = worldToCanvas(params.minX, y);
        ctx.beginPath();
        ctx.moveTo(offsetX, canvasPos.y);
        ctx.lineTo(offsetX + boardPixelWidth, canvasPos.y);
        ctx.stroke();
    }

    // Draw origin marker
    const origin = worldToCanvas(0, 0);
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

    // Draw target position (if set)
    const currentPos = worldToCanvas(currentPosition.x, currentPosition.y);
    const targetPos = worldToCanvas(targetPosition.x, targetPosition.y);
    
    // Calculate distance between current and target
    const dx = targetPosition.x - currentPosition.x;
    const dy = targetPosition.y - currentPosition.y;
    const distance = Math.sqrt(dx * dx + dy * dy);

    // Draw line from current to target if they're different
    if (distance > 1) {
        ctx.strokeStyle = '#0066ff';
        ctx.lineWidth = 1;
        ctx.setLineDash([5, 5]);
        ctx.beginPath();
        ctx.moveTo(currentPos.x, currentPos.y);
        ctx.lineTo(targetPos.x, targetPos.y);
        ctx.stroke();
        ctx.setLineDash([]);
    }

    // Draw target position marker (blue dashed circle)
    if (distance > 1) {
        ctx.strokeStyle = '#0066ff';
        ctx.lineWidth = 2;
        ctx.setLineDash([3, 3]);
        ctx.beginPath();
        ctx.arc(targetPos.x, targetPos.y, 12, 0, 2 * Math.PI);
        ctx.stroke();
        ctx.setLineDash([]);
    }

    // Draw current position marker (red crosshair)
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
