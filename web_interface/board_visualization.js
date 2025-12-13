/**
 * @file board_visualization.js
 * @brief Board visualization utilities for canvas rendering
 *
 * Provides functions to draw PCB boards with improved graphics including:
 * - Enhanced origin marker with axes
 * - Grid lines with measurements
 * - Better contrast and visibility
 */

/**
 * Configuration for board visualization
 */
const BoardVisualization = {
    // Colors
    colors: {
        background: '#1a1a1a',          // Dark background
        pcbBoard: '#1e4620',            // Dark green PCB
        pcbBorder: '#2d6830',           // Lighter green border
        gridMajor: '#2d5520',           // Major grid lines (every 50mm)
        gridMinor: '#243d18',           // Minor grid lines (every 10mm)
        originAxis: '#ffa500',          // Orange for origin axes
        originLabel: '#ffffff',         // White for origin label
        currentPosition: '#ff3333',     // Bright red for current position
        targetPosition: '#3399ff',      // Bright blue for target position
        movePath: '#66ccff',            // Light blue for movement path
        measurementText: '#cccccc'      // Light gray for measurements
    },

    // Dimensions
    padding: 50,                        // Canvas padding
    gridMajorSpacing: 50,              // Major grid every 50mm
    gridMinorSpacing: 10,              // Minor grid every 10mm
    originMarkerSize: 30,              // Origin crosshair size
    originCircleRadius: 5,             // Origin center circle
    positionMarkerSize: 12,            // Current/target position size
    
    // Fonts
    fonts: {
        origin: 'bold 14px monospace',
        measurements: '11px monospace',
        coordinates: '12px monospace'
    }
};

/**
 * Draw the PCB board background with grid
 * @param {CanvasRenderingContext2D} ctx - Canvas context
 * @param {Object} params - Visualization parameters
 */
function drawPCBBoard(ctx, params) {
    const { offsetX, offsetY, boardPixelWidth, boardPixelHeight, scale, minX, maxX, minY, maxY } = params;
    
    // Clear canvas
    ctx.fillStyle = BoardVisualization.colors.background;
    ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);

    // Draw PCB board background
    ctx.fillStyle = BoardVisualization.colors.pcbBoard;
    ctx.fillRect(offsetX, offsetY, boardPixelWidth, boardPixelHeight);

    // Draw board border
    ctx.strokeStyle = BoardVisualization.colors.pcbBorder;
    ctx.lineWidth = 2;
    ctx.strokeRect(offsetX, offsetY, boardPixelWidth, boardPixelHeight);

    // Helper function to convert world coordinates to canvas coordinates
    const worldToCanvas = (worldX, worldY) => {
        return {
            x: offsetX + (worldX - minX) * scale,
            y: offsetY + (worldY - minY) * scale
        };
    };

    // Draw minor grid lines (10mm)
    ctx.strokeStyle = BoardVisualization.colors.gridMinor;
    ctx.lineWidth = 0.5;
    
    for (let x = Math.ceil(minX / BoardVisualization.gridMinorSpacing) * BoardVisualization.gridMinorSpacing; 
         x <= maxX; 
         x += BoardVisualization.gridMinorSpacing) {
        if (x % BoardVisualization.gridMajorSpacing === 0) continue; // Skip major grid positions
        const canvasPos = worldToCanvas(x, minY);
        ctx.beginPath();
        ctx.moveTo(canvasPos.x, offsetY);
        ctx.lineTo(canvasPos.x, offsetY + boardPixelHeight);
        ctx.stroke();
    }

    for (let y = Math.ceil(minY / BoardVisualization.gridMinorSpacing) * BoardVisualization.gridMinorSpacing; 
         y <= maxY; 
         y += BoardVisualization.gridMinorSpacing) {
        if (y % BoardVisualization.gridMajorSpacing === 0) continue; // Skip major grid positions
        const canvasPos = worldToCanvas(minX, y);
        ctx.beginPath();
        ctx.moveTo(offsetX, canvasPos.y);
        ctx.lineTo(offsetX + boardPixelWidth, canvasPos.y);
        ctx.stroke();
    }

    // Draw major grid lines (50mm) with labels
    ctx.strokeStyle = BoardVisualization.colors.gridMajor;
    ctx.lineWidth = 1;
    ctx.fillStyle = BoardVisualization.colors.measurementText;
    ctx.font = BoardVisualization.fonts.measurements;
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';

    for (let x = Math.ceil(minX / BoardVisualization.gridMajorSpacing) * BoardVisualization.gridMajorSpacing; 
         x <= maxX; 
         x += BoardVisualization.gridMajorSpacing) {
        if (x === 0) continue; // Skip origin (will draw separately)
        const canvasPos = worldToCanvas(x, minY);
        ctx.beginPath();
        ctx.moveTo(canvasPos.x, offsetY);
        ctx.lineTo(canvasPos.x, offsetY + boardPixelHeight);
        ctx.stroke();
        
        // Label at top
        ctx.fillText(`${x}`, canvasPos.x, offsetY - 20);
    }

    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';

    for (let y = Math.ceil(minY / BoardVisualization.gridMajorSpacing) * BoardVisualization.gridMajorSpacing; 
         y <= maxY; 
         y += BoardVisualization.gridMajorSpacing) {
        if (y === 0) continue; // Skip origin (will draw separately)
        const canvasPos = worldToCanvas(minX, y);
        ctx.beginPath();
        ctx.moveTo(offsetX, canvasPos.y);
        ctx.lineTo(offsetX + boardPixelWidth, canvasPos.y);
        ctx.stroke();
        
        // Label at left
        ctx.fillText(`${y}`, offsetX - 8, canvasPos.y);
    }

    return worldToCanvas;
}

/**
 * Draw enhanced origin marker (0,0)
 * @param {CanvasRenderingContext2D} ctx - Canvas context
 * @param {Function} worldToCanvas - Coordinate conversion function
 */
function drawOriginMarker(ctx, worldToCanvas) {
    const origin = worldToCanvas(0, 0);
    const size = BoardVisualization.originMarkerSize;

    // Draw axes through origin
    ctx.strokeStyle = BoardVisualization.colors.originAxis;
    ctx.lineWidth = 2;
    ctx.setLineDash([]);

    // X-axis (horizontal)
    ctx.beginPath();
    ctx.moveTo(origin.x - size, origin.y);
    ctx.lineTo(origin.x + size, origin.y);
    ctx.stroke();

    // Y-axis (vertical)
    ctx.beginPath();
    ctx.moveTo(origin.x, origin.y - size);
    ctx.lineTo(origin.x, origin.y + size);
    ctx.stroke();

    // Draw arrowheads
    const arrowSize = 8;
    
    // X-axis arrow (right)
    ctx.beginPath();
    ctx.moveTo(origin.x + size, origin.y);
    ctx.lineTo(origin.x + size - arrowSize, origin.y - arrowSize / 2);
    ctx.lineTo(origin.x + size - arrowSize, origin.y + arrowSize / 2);
    ctx.closePath();
    ctx.fillStyle = BoardVisualization.colors.originAxis;
    ctx.fill();

    // Y-axis arrow (up - note: canvas Y increases downward, so we want negative Y to go up)
    ctx.beginPath();
    ctx.moveTo(origin.x, origin.y - size);
    ctx.lineTo(origin.x - arrowSize / 2, origin.y - size + arrowSize);
    ctx.lineTo(origin.x + arrowSize / 2, origin.y - size + arrowSize);
    ctx.closePath();
    ctx.fill();

    // Draw center circle
    ctx.fillStyle = BoardVisualization.colors.originAxis;
    ctx.beginPath();
    ctx.arc(origin.x, origin.y, BoardVisualization.originCircleRadius, 0, 2 * Math.PI);
    ctx.fill();

    // Draw outer circle
    ctx.strokeStyle = BoardVisualization.colors.originAxis;
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.arc(origin.x, origin.y, BoardVisualization.originCircleRadius + 3, 0, 2 * Math.PI);
    ctx.stroke();

    // Draw axis labels
    ctx.font = BoardVisualization.fonts.coordinates;
    ctx.fillStyle = BoardVisualization.colors.originAxis;

}

/**
 * Draw current position marker
 * @param {CanvasRenderingContext2D} ctx - Canvas context
 * @param {Function} worldToCanvas - Coordinate conversion function
 * @param {Object} position - Position {x, y} in mm
 */
function drawCurrentPosition(ctx, worldToCanvas, position) {
    const pos = worldToCanvas(position.x, position.y);
    const size = BoardVisualization.positionMarkerSize;

    // Draw crosshair
    ctx.strokeStyle = BoardVisualization.colors.currentPosition;
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(pos.x - size, pos.y);
    ctx.lineTo(pos.x + size, pos.y);
    ctx.moveTo(pos.x, pos.y - size);
    ctx.lineTo(pos.x, pos.y + size);
    ctx.stroke();

    // Draw circle
    ctx.strokeStyle = BoardVisualization.colors.currentPosition;
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.arc(pos.x, pos.y, size * 0.7, 0, 2 * Math.PI);
    ctx.stroke();

    // Draw center dot
    ctx.fillStyle = BoardVisualization.colors.currentPosition;
    ctx.beginPath();
    ctx.arc(pos.x, pos.y, 3, 0, 2 * Math.PI);
    ctx.fill();

    // Draw position label
    ctx.font = BoardVisualization.fonts.coordinates;
    ctx.textAlign = 'left';
    ctx.textBaseline = 'bottom';
    const label = `(${position.x.toFixed(1)}, ${position.y.toFixed(1)})`;
    
    // Background
    const metrics = ctx.measureText(label);
    const labelX = pos.x + size + 5;
    const labelY = pos.y - 5;
    ctx.fillStyle = 'rgba(255, 51, 51, 0.8)';
    ctx.fillRect(labelX - 2, labelY - 12, metrics.width + 4, 14);
    
    // Text
    ctx.fillStyle = '#ffffff';
    ctx.fillText(label, labelX, labelY);
}

/**
 * Draw target position marker
 * @param {CanvasRenderingContext2D} ctx - Canvas context
 * @param {Function} worldToCanvas - Coordinate conversion function
 * @param {Object} position - Position {x, y} in mm
 */
function drawTargetPosition(ctx, worldToCanvas, position) {
    const pos = worldToCanvas(position.x, position.y);
    const size = BoardVisualization.positionMarkerSize;

    // Draw dashed circle
    ctx.strokeStyle = BoardVisualization.colors.targetPosition;
    ctx.lineWidth = 2;
    ctx.setLineDash([4, 4]);
    ctx.beginPath();
    ctx.arc(pos.x, pos.y, size, 0, 2 * Math.PI);
    ctx.stroke();
    ctx.setLineDash([]);

    // Draw inner circle
    ctx.strokeStyle = BoardVisualization.colors.targetPosition;
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.arc(pos.x, pos.y, size * 0.5, 0, 2 * Math.PI);
    ctx.stroke();
}

/**
 * Draw movement path between current and target
 * @param {CanvasRenderingContext2D} ctx - Canvas context
 * @param {Function} worldToCanvas - Coordinate conversion function
 * @param {Object} current - Current position {x, y} in mm
 * @param {Object} target - Target position {x, y} in mm
 */
function drawMovementPath(ctx, worldToCanvas, current, target) {
    const currentPos = worldToCanvas(current.x, current.y);
    const targetPos = worldToCanvas(target.x, target.y);

    // Calculate distance
    const dx = target.x - current.x;
    const dy = target.y - current.y;
    const distance = Math.sqrt(dx * dx + dy * dy);

    // Only draw if positions are different
    if (distance < 0.1) return;

    // Draw dashed line
    ctx.strokeStyle = BoardVisualization.colors.movePath;
    ctx.lineWidth = 2;
    ctx.setLineDash([8, 8]);
    ctx.beginPath();
    ctx.moveTo(currentPos.x, currentPos.y);
    ctx.lineTo(targetPos.x, targetPos.y);
    ctx.stroke();
    ctx.setLineDash([]);

    // Draw distance label at midpoint
    const midX = (currentPos.x + targetPos.x) / 2;
    const midY = (currentPos.y + targetPos.y) / 2;
    
    ctx.font = BoardVisualization.fonts.coordinates;
    ctx.textAlign = 'center';
    ctx.textBaseline = 'bottom';
    
    const label = `${distance.toFixed(1)}mm`;
    const metrics = ctx.measureText(label);
    
    // Background
    ctx.fillStyle = 'rgba(51, 153, 255, 0.9)';
    ctx.fillRect(midX - metrics.width / 2 - 3, midY - 14, metrics.width + 6, 14);
    
    // Text
    ctx.fillStyle = '#ffffff';
    ctx.fillText(label, midX, midY);
}
