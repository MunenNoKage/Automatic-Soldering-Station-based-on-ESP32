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
