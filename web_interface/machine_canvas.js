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
 * @param {Object} limits - Coordinate limits {x: {min, max}, y: {min, max}, z: {min, max}}
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

    // Draw PCB board and get coordinate conversion function (use manual colors)
    const worldToCanvas = drawPCBBoard(ctx, vizParams, BoardVisualization.manualColors);

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

    // Draw Z-axis indicator
    const zMin = limits.z !== undefined ? limits.z.min : 0;
    const zMax = limits.z !== undefined ? limits.z.max : 180;
    drawZAxisIndicator(ctx, currentPosition.z, zMin, zMax, vizParams);
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

/**
 * Convert canvas coordinates to world coordinates for machine canvas
 * @param {HTMLCanvasElement} canvas - Canvas element
 * @param {number} canvasX - Canvas X coordinate (pixels)
 * @param {number} canvasY - Canvas Y coordinate (pixels)
 * @param {Object} limits - Coordinate limits {x: {min, max}, y: {min, max}}
 * @returns {Object} World coordinates {x, y} in mm
 */
function machineCanvasToWorld(canvas, canvasX, canvasY, limits) {
    // Handle both coordinate limit formats
    const minX = limits.minX !== undefined ? limits.minX : limits.x.min;
    const maxX = limits.maxX !== undefined ? limits.maxX : limits.x.max;
    const minY = limits.minY !== undefined ? limits.minY : limits.y.min;
    const maxY = limits.maxY !== undefined ? limits.maxY : limits.y.max;
    
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

    // Convert canvas coordinates to world coordinates
    const worldX = ((canvasX - offsetX) / scale) + minX;
    const worldY = ((canvasY - offsetY) / scale) + minY;

    return { x: worldX, y: worldY };
}

/**
 * Draw Z-axis position indicator on canvas
 * @param {CanvasRenderingContext2D} ctx - Canvas context
 * @param {number} zPosition - Current Z position in mm
 * @param {number} zMin - Minimum Z value (typically 0)
 * @param {number} zMax - Maximum Z value (typically 180)
 * @param {Object} vizParams - Visualization parameters from drawMachineCanvas
 */
function drawZAxisIndicator(ctx, zPosition, zMin, zMax, vizParams) {
    const canvas = ctx.canvas;
    const padding = BoardVisualization.padding;
    
    // Calculate available space on right side
    const boardRight = vizParams.offsetX + vizParams.boardPixelWidth;
    const canvasRight = canvas.width - padding;
    const availableWidth = canvasRight - boardRight;
    
    // Z-axis bar dimensions
    const barWidth = 30;
    const barHeight = vizParams.boardPixelHeight;
    const barX = boardRight + (availableWidth - barWidth) / 2;
    const barY = vizParams.offsetY;
    
    // Only draw if there's enough space
    if (availableWidth < 50) return;
    
    // Draw bar background
    ctx.fillStyle = '#2a2a2a';
    ctx.fillRect(barX, barY, barWidth, barHeight);
    
    // Draw bar border
    ctx.strokeStyle = '#444444';
    ctx.lineWidth = 2;
    ctx.strokeRect(barX, barY, barWidth, barHeight);
    
    // Draw scale marks and labels
    ctx.font = '10px Arial';
    ctx.fillStyle = '#888888';
    ctx.textAlign = 'left';
    
    // Top label (0mm)
    ctx.fillText('0mm', barX + barWidth + 10, barY + 12);
    
    // Bottom label (180mm)
    ctx.fillText(zMax + 'mm', barX + barWidth + 10, barY + barHeight - 5);
    
    // Calculate marker position (0mm at top, zMax at bottom)
    const zRange = zMax - zMin;
    const zNormalized = Math.max(0, Math.min(1, (zPosition - zMin) / zRange));
    const markerY = barY + zNormalized * barHeight;
    
    // Draw current position marker (horizontal line with arrows)
    const markerHeight = 3;
    const arrowSize = 5;
    
    // Marker line
    ctx.fillStyle = '#ffa500';
    ctx.fillRect(barX, markerY - markerHeight / 2, barWidth, markerHeight);
    
    // Left arrow
    ctx.beginPath();
    ctx.moveTo(barX, markerY);
    ctx.lineTo(barX - arrowSize, markerY - arrowSize);
    ctx.lineTo(barX - arrowSize, markerY + arrowSize);
    ctx.closePath();
    ctx.fill();
    
    // Right arrow
    ctx.beginPath();
    ctx.moveTo(barX + barWidth, markerY);
    ctx.lineTo(barX + barWidth + arrowSize, markerY - arrowSize);
    ctx.lineTo(barX + barWidth + arrowSize, markerY + arrowSize);
    ctx.closePath();
    ctx.fill();
    
    // Draw current value above marker
    ctx.font = 'bold 12px Arial';
    ctx.fillStyle = '#ffffff';
    ctx.textAlign = 'center';
    const valueText = zPosition.toFixed(1) + 'mm';
    const valueY = markerY < barY + 20 ? markerY + 20 : markerY - 10;
    ctx.fillText(valueText, barX + barWidth / 2, valueY);
}
