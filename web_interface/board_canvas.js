/**
 * @file board_canvas.js
 * @brief Canvas visualization for PCB board with drill holes (index.html and exec.html)
 *
 * Provides visualization for drill files with PCB board representation,
 * drill holes, and current position tracking during execution.
 */

/**
 * Draw PCB board canvas with drill holes
 * @param {CanvasRenderingContext2D} ctx - Canvas context
 * @param {Object} params - Visualization parameters
 * @param {Array} drillPoints - Array of drill hole coordinates {x, y}
 * @param {Object} currentPosition - Current machine position {x, y}
 * @param {Set} excludedHoles - Set of excluded hole indices (optional)
 * @param {number} hoveredIndex - Index of hovered hole (optional, -1 for none)
 */
function drawBoardCanvas(ctx, params, drillPoints, currentPosition, excludedHoles = new Set(), hoveredIndex = -1) {
    // Handle null excludedHoles
    if (!excludedHoles) {
        excludedHoles = new Set();
    }
    
    const { minX, minY, maxX, maxY, marginRight, marginBottom, scale, offsetX, offsetY, 
            canvasWidth, canvasHeight, scaledBoardWidth, scaledBoardHeight, boardWidth, boardHeight } = params;

    // Clear canvas
    ctx.fillStyle = BoardVisualization.colors.background;
    ctx.fillRect(0, 0, canvasWidth, canvasHeight);

    // Draw PCB board
    ctx.fillStyle = BoardVisualization.colors.pcbBoard;
    ctx.fillRect(offsetX, offsetY, scaledBoardWidth, scaledBoardHeight);

    // Draw board border
    ctx.strokeStyle = BoardVisualization.colors.pcbBorder;
    ctx.lineWidth = 2;
    ctx.strokeRect(offsetX, offsetY, scaledBoardWidth, scaledBoardHeight);

    // Draw minor grid lines
    ctx.strokeStyle = BoardVisualization.colors.gridMinor;
    ctx.lineWidth = 0.5;
    const gridMinor = BoardVisualization.gridMinorSpacing;

    for (let x = 0; x <= boardWidth; x += gridMinor) {
        const canvasX = offsetX + x * scale;
        ctx.beginPath();
        ctx.moveTo(canvasX, offsetY);
        ctx.lineTo(canvasX, offsetY + scaledBoardHeight);
        ctx.stroke();
    }

    for (let y = 0; y <= boardHeight; y += gridMinor) {
        const canvasY = offsetY + y * scale;
        ctx.beginPath();
        ctx.moveTo(offsetX, canvasY);
        ctx.lineTo(offsetX + scaledBoardWidth, canvasY);
        ctx.stroke();
    }

    // Draw major grid lines with labels
    ctx.strokeStyle = BoardVisualization.colors.gridMajor;
    ctx.lineWidth = 1;
    ctx.fillStyle = BoardVisualization.colors.measurementText;
    ctx.font = BoardVisualization.fonts.measurements;
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';

    const gridMajor = BoardVisualization.gridMajorSpacing;
    for (let x = gridMajor; x <= boardWidth; x += gridMajor) {
        const canvasX = offsetX + x * scale;
        ctx.beginPath();
        ctx.moveTo(canvasX, offsetY);
        ctx.lineTo(canvasX, offsetY + scaledBoardHeight);
        ctx.stroke();
        ctx.fillText(`${x}`, canvasX, offsetY - 20);
    }

    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';
    for (let y = gridMajor; y <= boardHeight; y += gridMajor) {
        const canvasY = offsetY + y * scale;
        ctx.beginPath();
        ctx.moveTo(offsetX, canvasY);
        ctx.lineTo(offsetX + scaledBoardWidth, canvasY);
        ctx.stroke();
        ctx.fillText(`${y}`, offsetX - 8, canvasY);
    }

    // Helper function to convert world coordinates to canvas coordinates
    // No margin shift - origin (0,0) should be at the edge of the board
    const worldToCanvas = (worldX, worldY) => {
        return {
            x: offsetX + (worldX - minX) * scale,
            y: offsetY + (worldY - minY) * scale
        };
    };

    // Draw origin marker at actual (0, 0) coordinates
    drawBoardOriginMarker(ctx, worldToCanvas, canvasWidth, canvasHeight);

    // Draw drill holes
    for (let i = 0; i < drillPoints.length; i++) {
        const point = drillPoints[i];
        const pos = worldToCanvas(point.x, point.y);
        const isExcluded = excludedHoles.has(i);
        const isHovered = (i === hoveredIndex);

        // Gold pad (or gray if excluded)
        if (isExcluded) {
            ctx.fillStyle = '#616161ff';
        } else {
            ctx.fillStyle = isHovered ? '#ffd700' : '#d4af37';
        }
        const padRadius = isHovered ? 10 : 8;
        ctx.beginPath();
        ctx.arc(pos.x, pos.y, padRadius, 0, 2 * Math.PI);
        ctx.fill();

        // Black center
        ctx.fillStyle = '#000000';
        ctx.beginPath();
        ctx.arc(pos.x, pos.y, 3, 0, 2 * Math.PI);
        ctx.fill();

        // Hole number (if not excluded and not too many holes)
        if (!isExcluded && drillPoints.length <= 50) {
            ctx.fillStyle = '#ffffffff';
            ctx.font = '10px monospace';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'top';
            ctx.fillText((i + 1).toString(), pos.x, pos.y + padRadius + 3);
        }
    }

    // Draw hover tooltip
    if (hoveredIndex >= 0 && hoveredIndex < drillPoints.length) {
        const point = drillPoints[hoveredIndex];
        const pos = worldToCanvas(point.x, point.y);
        const isExcluded = excludedHoles.has(hoveredIndex);

        const tooltipText = isExcluded 
            ? `Hole ${hoveredIndex + 1} - EXCLUDED`
            : `Hole ${hoveredIndex + 1}: (${point.x.toFixed(2)}, ${point.y.toFixed(2)}) mm`;
        
        ctx.font = '14px Arial';
        const textMetrics = ctx.measureText(tooltipText);
        const tooltipWidth = textMetrics.width + 16;
        const tooltipHeight = 24;
        
        let tooltipX = pos.x - tooltipWidth / 2;
        let tooltipY = pos.y - 40;
        
        // Keep tooltip within canvas bounds
        if (tooltipX < 5) tooltipX = 5;
        if (tooltipX + tooltipWidth > canvasWidth - 5) tooltipX = canvasWidth - tooltipWidth - 5;
        if (tooltipY < 5) tooltipY = pos.y + 20;
        
        // Draw tooltip background
        ctx.fillStyle = 'rgba(0, 0, 0, 0.85)';
        ctx.strokeStyle = isExcluded ? '#cc0000' : '#ffd700';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.roundRect(tooltipX, tooltipY, tooltipWidth, tooltipHeight, 4);
        ctx.fill();
        ctx.stroke();
        
        // Draw tooltip text
        ctx.fillStyle = '#ffffff';
        ctx.textAlign = 'left';
        ctx.textBaseline = 'middle';
        ctx.fillText(tooltipText, tooltipX + 8, tooltipY + tooltipHeight / 2);
    }

    // Draw current position marker
    if (currentPosition) {
        const pos = worldToCanvas(currentPosition.x, currentPosition.y);
        const markerSize = BoardVisualization.positionMarkerSize;
        
        // Red crosshair
        ctx.strokeStyle = BoardVisualization.colors.currentPosition;
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(pos.x - markerSize, pos.y);
        ctx.lineTo(pos.x + markerSize, pos.y);
        ctx.moveTo(pos.x, pos.y - markerSize);
        ctx.lineTo(pos.x, pos.y + markerSize);
        ctx.stroke();

        // Red circle
        ctx.beginPath();
        ctx.arc(pos.x, pos.y, markerSize * 0.7, 0, 2 * Math.PI);
        ctx.stroke();

        // Center dot
        ctx.fillStyle = BoardVisualization.colors.currentPosition;
        ctx.beginPath();
        ctx.arc(pos.x, pos.y, 3, 0, 2 * Math.PI);
        ctx.fill();

        // Position label
        ctx.font = BoardVisualization.fonts.coordinates;
        ctx.textAlign = 'left';
        ctx.textBaseline = 'bottom';
        const label = `(${currentPosition.x.toFixed(1)}, ${currentPosition.y.toFixed(1)})`;
        const metrics = ctx.measureText(label);
        const labelX = pos.x + markerSize + 5;
        const labelY = pos.y - 5;
        
        ctx.fillStyle = 'rgba(255, 51, 51, 0.8)';
        ctx.fillRect(labelX - 2, labelY - 12, metrics.width + 4, 14);
        ctx.fillStyle = '#ffffff';
        ctx.fillText(label, labelX, labelY);
    }
}

/**
 * Draw origin marker on board canvas
 * @param {CanvasRenderingContext2D} ctx - Canvas context
 * @param {Function} worldToCanvas - Coordinate conversion function
 * @param {number} canvasWidth - Canvas width
 * @param {number} canvasHeight - Canvas height
 */
function drawBoardOriginMarker(ctx, worldToCanvas, canvasWidth, canvasHeight) {
    const origin = worldToCanvas(0, 0);
    
    // Check if origin is within canvas bounds
    if (origin.x < 0 || origin.x > canvasWidth || origin.y < 0 || origin.y > canvasHeight) {
        return;
    }
    
    const size = BoardVisualization.originMarkerSize;
}

/**
 * Calculate visualization parameters for board canvas
 * @param {HTMLCanvasElement} canvas - Canvas element
 * @param {Array} drillPoints - Array of drill hole coordinates {x, y}
 * @param {number} margin - Margin around board in mm (not used, kept for compatibility)
 * @returns {Object} Visualization parameters
 */
function calculateBoardCanvasParams(canvas, drillPoints, margin = 10) {
    // Calculate board dimensions from drill points, always including origin (0, 0)
    let minX = 0, maxX = 0;
    let minY = 0, maxY = 0;

    for (const point of drillPoints) {
        minX = Math.min(minX, point.x);
        maxX = Math.max(maxX, point.x);
        minY = Math.min(minY, point.y);
        maxY = Math.max(maxY, point.y);
    }

    // Calculate smart margin: distance from top-left hole to origin
    // This creates symmetrical padding on right/bottom sides
    const topLeftHoleX = Math.min(...drillPoints.map(p => p.x));
    const topLeftHoleY = Math.min(...drillPoints.map(p => p.y));
    const marginRight = topLeftHoleX - minX;  // Distance from origin to first hole on X
    const marginBottom = topLeftHoleY - minY; // Distance from origin to first hole on Y

    // Board dimensions with asymmetric margins (only on right and bottom)
    const boardWidth = (maxX - minX) + marginRight;
    const boardHeight = (maxY - minY) + marginBottom;

    // Canvas dimensions
    const canvasWidth = canvas.width;
    const canvasHeight = canvas.height;

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

    return {
        minX, minY, maxX, maxY,
        margin: marginRight, // Use marginRight as the primary margin value for compatibility
        marginRight, marginBottom,
        scale,
        offsetX, offsetY,
        canvasWidth, canvasHeight,
        scaledBoardWidth, scaledBoardHeight,
        boardWidth, boardHeight
    };
}

/**
 * Convert canvas coordinates to world coordinates
 * @param {number} canvasX - Canvas X coordinate
 * @param {number} canvasY - Canvas Y coordinate
 * @param {Object} params - Visualization parameters
 * @returns {Object} World coordinates {x, y}
 */
function canvasToWorld(canvasX, canvasY, params) {
    const { minX, minY, scale, offsetX, offsetY } = params;
    const worldX = ((canvasX - offsetX) / scale) + minX;
    const worldY = ((canvasY - offsetY) / scale) + minY;
    return { x: worldX, y: worldY };
}

/**
 * Find hovered drill hole index
 * @param {number} canvasX - Mouse X on canvas
 * @param {number} canvasY - Mouse Y on canvas
 * @param {Array} drillPoints - Array of drill points
 * @param {Object} params - Visualization parameters
 * @param {number} tolerance - Detection tolerance in pixels
 * @returns {number} Hole index or -1 if none
 */
function findHoveredHole(canvasX, canvasY, drillPoints, params, tolerance = 15) {
    const { minX, minY, scale, offsetX, offsetY } = params;
    
    for (let i = 0; i < drillPoints.length; i++) {
        const point = drillPoints[i];
        const x = offsetX + (point.x - minX) * scale;
        const y = offsetY + (point.y - minY) * scale;
        
        const distance = Math.sqrt((canvasX - x) ** 2 + (canvasY - y) ** 2);
        if (distance <= tolerance) {
            return i;
        }
    }
    
    return -1;
}
