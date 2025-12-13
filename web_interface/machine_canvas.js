/**
 * @file machine_canvas.js
 * @brief Canvas visualization for machine working area (manual.html)
 *
 * Provides visualization for the full machine working area showing
 * current position, target position, and movement paths.
 */

/**
 * Draw machine working area canvas
 * @param {CanvasRenderingContext2D} ctx - Canvas context
 * @param {Object} limits - Coordinate limits {x: {min, max}, y: {min, max}} or {minX, minY, maxX, maxY}
 * @param {Object} currentPosition - Current position {x, y, z}
 * @param {Object} targetPosition - Target position {x, y, z}
 */
function drawMachineCanvas(ctx, limits, currentPosition, targetPosition) {
    // Handle both coordinate limit formats
    const minX = limits.minX !== undefined ? limits.minX : limits.x.min;
    const maxX = limits.maxX !== undefined ? limits.maxX : limits.x.max;
    const minY = limits.minY !== undefined ? limits.minY : limits.y.min;
    const maxY = limits.maxY !== undefined ? limits.maxY : limits.y.max;
    
    const canvas = ctx.canvas;
    const padding = BoardVisualization.padding;
    const drawWidth = canvas.width - 2 * padding;
    const drawHeight = canvas.height - 2 * padding;

    const boardWidth = maxX - minX;
    const boardHeight = maxY - minY;

    // Calculate scale
    const scaleX = drawWidth / boardWidth;
    const scaleY = drawHeight / boardHeight;
    const scale = Math.min(scaleX, scaleY);

    // Center the working area
    const boardPixelWidth = boardWidth * scale;
    const boardPixelHeight = boardHeight * scale;
    const offsetX = padding + (drawWidth - boardPixelWidth) / 2;
    const offsetY = padding + (drawHeight - boardPixelHeight) / 2;

    // Create visualization parameters
    const vizParams = {
        offsetX,
        offsetY,
        boardPixelWidth,
        boardPixelHeight,
        scale,
        minX: minX,
        maxX: maxX,
        minY: minY,
        maxY: maxY
    };

    // Draw PCB board and get coordinate conversion function
    const worldToCanvas = drawPCBBoard(ctx, vizParams);

    // Draw enhanced origin marker
    drawOriginMarker(ctx, worldToCanvas);

    // Calculate distance between current and target
    const dx = targetPosition.x - currentPosition.x;
    const dy = targetPosition.y - currentPosition.y;
    const distance = Math.sqrt(dx * dx + dy * dy);

    // Draw movement path if positions are different
    if (distance > 0.1) {
        drawMovementPath(ctx, worldToCanvas, currentPosition, targetPosition);
        drawTargetPosition(ctx, worldToCanvas, targetPosition);
    }

    // Draw current position marker
    drawCurrentPosition(ctx, worldToCanvas, currentPosition);
}

/**
 * Create machine canvas parameters
 * @param {Object} coordinateLimits - Coordinate limits {x: {min, max}, y: {min, max}, z: {min, max}}
 * @returns {Object} Visualization parameters
 */
function createMachineCanvasParams(coordinateLimits) {
    return {
        minX: coordinateLimits.x.min,
        minY: coordinateLimits.y.min,
        maxX: coordinateLimits.x.max,
        maxY: coordinateLimits.y.max,
        boardWidth: coordinateLimits.x.max - coordinateLimits.x.min,
        boardHeight: coordinateLimits.y.max - coordinateLimits.y.min
    };
}
