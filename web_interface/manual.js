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
        } else {
            manualStatus.textContent = 'Error: ' + (result.message || 'Failed to exit manual mode');
            manualStatus.className = 'upload-status error';
        }
    } catch (error) {
        manualStatus.textContent = 'Error: ' + error.message;
        manualStatus.className = 'upload-status error';
    }
}
