
// Global parameters ----------------------------------------------------------------------------------------------------------------------------

const IMAGE_RENDER_SCALE = 0.25;
const LENGTH_SCALE       = 5.0 / (400 * IMAGE_RENDER_SCALE);

const MAX_RECTS = 6;
const GRID_SIZE = 10;

const ROT_FINE    = 5 * (Math.PI / 180);
const ROT_COARSE  = 2 * ROT_FINE;
const MOVE_FINE   = 1;
const MOVE_COARSE = 10;

const LAYER_PRIORITY = {
    barricade: 3,
    car:       2,
    spikes:    1
};

const RECT_CONFIG = {
    car:       { src: 'Assets/Images/Car.webp' },
    spikes:    { src: 'Assets/Images/Spikes.webp' },
    barricade: { src: 'Assets/Images/Barricade.webp' }
};

const TYPE_MAP = {
    1: 'car',
    2: 'barricade',
    3: 'spikes'
};





// Global objects -------------------------------------------------------------------------------------------------------------------------------

const canvas        = document.getElementById('mainCanvas');
const ctx           = canvas.getContext('2d');
const outputField   = document.getElementById('outputField');
const outputOverlay = document.getElementById('outputOverlay');
const counterEl     = document.getElementById('canvasCounter');
const canvasOverlay = document.getElementById('canvasOverlay');
const modal         = document.getElementById('importModal');
const importInput   = document.getElementById('importInput');
const btnLoadImport = document.getElementById('btnLoadImport');





// Global state ---------------------------------------------------------------------------------------------------------------------------------

const images     = {};
let imagesLoaded = 0;

let rects           = [];
let selectedIndices = [];

let isDraggingGroup = false;
let isBoxSelecting  = false;

let boxStart = {
    x: 0,
    y: 0
};

let boxEnd = {
    x: 0,
    y: 0
};

let dragStartData     = null;
let isShiftDown       = false;
let selectionSnapshot = [];
let layerCounter      = 0;
let clipboard         = null;





// Initialisation -------------------------------------------------------------------------------------------------------------------------------

Object.keys(RECT_CONFIG).forEach(key => {
    const img = new Image();
    img.src   = RECT_CONFIG[key].src;
    
    img.onload = () => {
        if (++imagesLoaded === 3) initDefault();
    };
    
    images[key] = img;
});





// Functions and listeners  ---------------------------------------------------------------------------------------------------------------------

function initDefault() {
    addRect('car');
    draw();
}



function openImportModal() {
    modal.style.display    = 'flex';
    btnLoadImport.disabled = true;
    importInput.value      = '';
    importInput.focus();
}



function closeImportModal() {
    modal.style.display = 'none';
    canvas.focus();
}



modal.addEventListener('mousedown', (e) => {
    if (e.target === modal) closeImportModal();
});



function parseImportLines(text) {
    return text.split('\n')
        .map(l => l.trim())
        .filter(l => l.startsWith('part'));
}



importInput.addEventListener('input', () => {
    const lines = parseImportLines(importInput.value);
    let isValid = true;
    let hasCar  = false;
    
    if (lines.length === 0 || lines.length > MAX_RECTS)
        isValid = false;
    
    else
    {
        const regex = /^\s*part(0[1-6])\s*=\s*([1-3])\s*,\s*(-?\d+(\.\d+)?)\s*,\s*(-?\d+(\.\d+)?)\s*,\s*(-?\d+(\.\d+)?)\s*$/;
	
        for (let i = 0; i < lines.length; i++)
	{
            const line  = lines[i];
            const match = line.match(regex);
	    
            if (!match)
	    {
                isValid = false;
                break;
            }
	    
            const partNum = parseInt(match[1], 10);
	    
            if (partNum !== (i + 1))
	    {
                isValid = false;
                break;
            }
	    
            if (parseInt(match[2]) === 1) hasCar = true;
        }
    }
    
    btnLoadImport.disabled = !(isValid && hasCar);
});



function loadImportData() {
    const regex = /^\s*part(0[1-6])\s*=\s*([1-3])\s*,\s*(-?\d+(\.\d+)?)\s*,\s*(-?\d+(\.\d+)?)\s*,\s*(-?\d+(\.\d+)?)\s*$/;

    const lines     = parseImportLines(importInput.value);
    const tempRects = [];
    
    let localLayerCounter = 0;
    
    lines.forEach(line => {
        const match = line.match(regex);
	
        const typeCode = parseInt(match[2]);
        const offX     = parseFloat(match[3]);
        const offY     = parseFloat(match[5]);
        const deg      = parseFloat(match[7]);
        const type     = TYPE_MAP[typeCode];
        const img      = images[type];
	
        let degrees  = deg * 360;
        let rawDeg   = (type === 'car') ? degrees : (degrees - 90);
        let angleRad = (90 - rawDeg) * (Math.PI / 180);
	
        tempRects.push({
            x:     (offX / LENGTH_SCALE),
            y:     -(offY / LENGTH_SCALE),
            w:     img.width * IMAGE_RENDER_SCALE,
            h:     img.height * IMAGE_RENDER_SCALE,
            img:   img,
            type:  type,
            angle: angleRad,
            layer: ++localLayerCounter
        });
    });
    
    let minX = Infinity;
    let maxX = -Infinity;
    let minY = Infinity;
    let maxY = -Infinity;
    
    tempRects.forEach(r => {
        const corners = getLocalCorners(r);
	
        corners.forEach(c => {
            minX = Math.min(minX, r.x + c.x);
            maxX = Math.max(maxX, r.x + c.x);
            minY = Math.min(minY, r.y + c.y);
            maxY = Math.max(maxY, r.y + c.y);
        });
    });
    
    const mbbCx    = (minX + maxX) / 2;
    const mbbCy    = (minY + maxY) / 2;
    const canvasCx = canvas.width / 2;
    const canvasCy = canvas.height / 2;
    
    rects = tempRects.map(r => ({
        ...r,
        x: r.x - mbbCx + canvasCx,
        y: r.y - mbbCy + canvasCy
    }));
    
    let isOutOfBounds = false;
    
    rects.forEach(r => {
        const corners = getLocalCorners(r);
	
        corners.forEach(c => {
            const gx = r.x + c.x;
            const gy = r.y + c.y;
	    
            if (gx < 0 || gx > canvas.width || gy < 0 || gy > canvas.height) isOutOfBounds = true;
        });
    });
    
    layerCounter    = localLayerCounter;
    selectedIndices = [];
    
    closeImportModal();
    updateUI();
    draw();
    
    const defaultText = "Left-click to select. Scroll to rotate. Hold Shift to snap.";
    
    if (isOutOfBounds)
        canvasOverlay.innerHTML = 'Import <span style="color: #D61E00"><b>out of bounds</b></span>.';
    else
        canvasOverlay.innerHTML = 'Import <span style="color: #00A34C"><b>successful</b></span>.';
    
    setTimeout(() => {
        canvasOverlay.innerText   = defaultText;
        canvasOverlay.style.color = "#666";
    }, 4000);
}



document.addEventListener('mousedown', (e) => {
    if (e.target !== canvas && e.target.tagName !== 'BUTTON' && e.target !== outputField && !modal.contains(e.target) && e.target !== modal)
    {
        selectedIndices = [];
        updateUI();
        draw();
    }
});



outputField.addEventListener('click', () => {
    navigator.clipboard.writeText(outputField.innerText).then(() => {
        outputOverlay.innerHTML = 'Settings <span style="color: #DEB700"><b>copied</b></span>.';
        setTimeout(() => {
            outputOverlay.innerText = "Click to copy settings.";
        }, 4000);
    });
});



window.addEventListener('keydown', (e) => {
    if (e.key === 'Escape')
    {
        if (modal.style.display === 'flex')
	{
            closeImportModal();
            return;
        }
	
        if (document.activeElement === canvas)
	{
            selectedIndices = [];
            updateUI();
            draw();
        }
	
        return;
    }
    
    if (modal.style.display === 'flex') return;
    
    const isCanvasActive = (document.activeElement === canvas);
    
    if (e.key === 'Shift')
    {
        isShiftDown = true;
        updateUI();
        draw();
    }
    
    if (e.key === 'Delete' || e.key === 'Backspace') deleteSelected();
    
    if (e.ctrlKey && e.key.toLowerCase() === 'a' && isCanvasActive)
    {
        e.preventDefault();
        selectedIndices = rects.map((_, i) => i);
        updateUI();
        draw();
    }
    
    if (e.ctrlKey && e.key.toLowerCase() === 'c' && selectedIndices.length > 0)
        clipboard = selectedIndices.map(i => ({...rects[i]}));
    
    if (e.ctrlKey && e.key.toLowerCase() === 'v' && clipboard)
    {
        if (rects.length + clipboard.length <= MAX_RECTS)
	{
            const next = [];
	    
            clipboard.forEach(c => {
                layerCounter++;
		
                const nr = {
                    ...c,
                    x:     c.x + 15,
                    y:     c.y + 15,
                    layer: layerCounter
                };
		
                if (!canExistAt(nr, nr.x, nr.y, nr.angle)) {
                    nr.x = canvas.width / 2;
                    nr.y = canvas.height / 2;
                }
		
                rects.push(nr);
                next.push(rects.length - 1);
            });
	    
            selectedIndices = next;
            updateUI();
            draw();
        }
    }
    
    if (['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(e.key) && selectedIndices.length > 0 && isCanvasActive)
    {
        e.preventDefault();
	
        const step = isShiftDown ? MOVE_COARSE : MOVE_FINE;
        let limit  = step;
	
        selectedIndices.forEach(idx => {
            const r = rects[idx];
	    
            const corners = getLocalCorners(r).map(c => ({
                x: c.x + r.x,
                y: c.y + r.y
            }));
	    
            if (e.key === 'ArrowUp')    limit = Math.min(limit, Math.min(...corners.map(c => c.y)));
            if (e.key === 'ArrowDown')  limit = Math.min(limit, canvas.height - Math.max(...corners.map(c => c.y)));
            if (e.key === 'ArrowLeft')  limit = Math.min(limit, Math.min(...corners.map(c => c.x)));
            if (e.key === 'ArrowRight') limit = Math.min(limit, canvas.width - Math.max(...corners.map(c => c.x)));
        });
	
        if (limit > 0)
	{
            selectedIndices.forEach(idx => {
                if (e.key === 'ArrowUp')    rects[idx].y -= limit;
                if (e.key === 'ArrowDown')  rects[idx].y += limit;
                if (e.key === 'ArrowLeft')  rects[idx].x -= limit;
                if (e.key === 'ArrowRight') rects[idx].x += limit;
            });
	    
            draw();
        }
    }
});



window.addEventListener('keyup', (e) => {
    if (e.key === 'Shift')
    {
        isShiftDown = false;
        updateUI();
        draw();
    }
});



canvas.addEventListener('mousedown', (e) => {
    canvas.focus();
    
    const b  = canvas.getBoundingClientRect();
    const mx = e.clientX - b.left;
    const my = e.clientY - b.top;
    
    let hitIdx = -1;
    
    const sorted = rects.map((r, i) => ({r, i})).sort((a, b) => (LAYER_PRIORITY[b.r.type] - LAYER_PRIORITY[a.r.type]) || (b.r.layer - a.r.layer));
    
    for (let item of sorted)
    {
        if (isHit(item.r, mx, my))
	{
            hitIdx = item.i;
            break;
        }
    }
    
    if (hitIdx !== -1)
    {
        if (!isShiftDown && !selectedIndices.includes(hitIdx))
            selectedIndices = [hitIdx];
	
        else if (isShiftDown && !selectedIndices.includes(hitIdx))
            selectedIndices.push(hitIdx);
	
        layerCounter++;
        rects[hitIdx].layer = layerCounter;
        isDraggingGroup     = true;
	
        let minX = Math.min(...selectedIndices.map(i => rects[i].x));
        let minY = Math.min(...selectedIndices.map(i => rects[i].y));
	
        dragStartData = {
            groupMinX:    minX,
            groupMinY:    minY,
            mouseOffsetX: mx - minX,
            mouseOffsetY: my - minY,
            itemStarts:   selectedIndices.map(idx => ({
                idx,
                x: rects[idx].x,
                y: rects[idx].y
            }))
        };
    }
    else
    {
        if (!isShiftDown) selectedIndices = [];
	
        selectionSnapshot = [...selectedIndices];
        isBoxSelecting    = true;
	
        boxStart = {
            x: mx,
            y: my
        };
	
        boxEnd = {
            x: mx,
            y: my
        };
    }
    
    updateUI();
    draw();
});



window.addEventListener('mousemove', (e) => {
    const b  = canvas.getBoundingClientRect();
    const mx = e.clientX - b.left;
    const my = e.clientY - b.top;
    
    if (isDraggingGroup && dragStartData)
    {
        let tx = mx - dragStartData.mouseOffsetX;
        let ty = my - dragStartData.mouseOffsetY;
	
        if (isShiftDown)
	{
            tx = Math.round(tx / GRID_SIZE) * GRID_SIZE;
            ty = Math.round(ty / GRID_SIZE) * GRID_SIZE;
        }
	
        const dx = tx - dragStartData.groupMinX;
        const dy = ty - dragStartData.groupMinY;
	
        const props = dragStartData.itemStarts.map(p => ({
            idx: p.idx,
            x: p.x + dx,
            y: p.y + dy
        }));
	
        let sL = 0;
        let sR = 0;
        let sU = 0;
        let sD = 0;
	
        props.forEach(p => {
            const corners = getLocalCorners(rects[p.idx]);
	    
            const minLX = Math.min(...corners.map(c => c.x));
            const maxLX = Math.max(...corners.map(c => c.x));
            const minLY = Math.min(...corners.map(c => c.y));
            const maxLY = Math.max(...corners.map(c => c.y));
	    
            if (p.x + minLX < 0)             sR = Math.max(sR, -(p.x + minLX));
            if (p.x + maxLX > canvas.width)  sL = Math.min(sL, canvas.width - (p.x + maxLX));
            if (p.y + minLY < 0)             sD = Math.max(sD, -(p.y + minLY));
            if (p.y + maxLY > canvas.height) sU = Math.min(sU, canvas.height - (p.y + maxLY));
        });
	
        props.forEach(p => {
            rects[p.idx].x = p.x + sR + sL;
            rects[p.idx].y = p.y + sD + sU;
        });
    }
    else if (isBoxSelecting)
    {
        boxEnd = {
            x: mx,
            y: my
        };
	
        const xMin = Math.min(boxStart.x, boxEnd.x);
        const xMax = Math.max(boxStart.x, boxEnd.x);
        const yMin = Math.min(boxStart.y, boxEnd.y);
        const yMax = Math.max(boxStart.y, boxEnd.y);
	
        const inBox = rects.map((r, i) => (r.x >= xMin && r.x <= xMax && r.y >= yMin && r.y <= yMax ? i : -1)).filter(i => i !== -1);
	
        selectedIndices = Array.from(new Set([...selectionSnapshot, ...inBox]));
        updateUI();
    }
    
    draw();
});



window.addEventListener('mouseup', () => {
    isDraggingGroup = false;
    isBoxSelecting  = false;
    draw();
});



canvas.addEventListener('wheel', (e) => {
    if (selectedIndices.length > 0)
    {
        e.preventDefault();
	
        const step      = isShiftDown ? ROT_COARSE : ROT_FINE;
        const direction = e.deltaY > 0 ? 1 : -1;
	
        if (selectedIndices.every(idx => {
            const r      = rects[idx];
            let newAngle = r.angle + (direction * step);
            newAngle     = Math.round(newAngle / step) * step;
            return canExistAt(r, r.x, r.y, newAngle);
	}))
	{
            selectedIndices.forEach(idx => {
                let newAngle = rects[idx].angle + (direction * step);
                rects[idx].angle = Math.round(newAngle / step) * step;
            });
	    
            draw();
        }
    }
}, { passive: false });



function canExistAt(r, nx, ny, na)
{
    const cos = Math.cos(na);
    const sin = Math.sin(na);
    const hw  = r.w / 2;
    const hh  = r.h / 2;
    
    const corners = [{
        x: -hw * cos + hh * sin,
        y: -hw * sin - hh * cos
    }, {
        x: hw * cos + hh * sin,
        y: hw * sin - hh * cos
    }, {
        x: hw * cos - hh * sin,
        y: hw * sin + hh * cos
    }, {
        x: -hw * cos - hh * sin,
        y: -hw * sin + hh * cos
    }];
    
    return corners.every(c => (nx + c.x >= 0 && nx + c.x <= canvas.width && ny + c.y >= 0 && ny + c.y <= canvas.height));
}



function handleAction(type)	    
{
    if (isShiftDown && selectedIndices.length > 0)
    {
        const allY    = rects.reduce((acc, r, i) => (r.type === 'car' ? acc.concat(i) : acc), []);
        const selY    = selectedIndices.filter(i => rects[i].type === 'car');
        const protIdx = (allY.length > 0 && allY.length === selY.length && type !== 'car') ? selY[0] : -1;
	
        selectedIndices.forEach(idx => {
            if (idx !== protIdx) transformRect(idx, type);
        });
    }
    else
        addRect(type);
    
    updateUI();
    draw();
    canvas.focus();
}



function addRect(type)	    
{
    if (rects.length < MAX_RECTS)
    {
        const img = images[type];
        layerCounter++;
	
        rects.push({
            x:     canvas.width / 2,
            y:     canvas.height / 2,
            w:     img.width * IMAGE_RENDER_SCALE,
            h:     img.height * IMAGE_RENDER_SCALE,
            img,
            type,
            angle: 0,
            layer: layerCounter
        });
	
        selectedIndices = [rects.length - 1];
    }
}



function transformRect(idx, newType)   
{
    const r   = rects[idx];
    const img = images[newType];
    
    r.type = newType;
    r.img  = img;
    r.w    = img.width * IMAGE_RENDER_SCALE;
    r.h    = img.height * IMAGE_RENDER_SCALE;
    
    if (!canExistAt(r, r.x, r.y, r.angle))
    {
        r.x = canvas.width  / 2;
        r.y = canvas.height / 2;
    }
}



function deleteSelected()
{
    if (selectedIndices.length === 0) return;
    
    const allY = rects.reduce((acc, r, i) => (r.type === 'car' ? acc.concat(i) : acc), []);
    const selY = selectedIndices.filter(i => rects[i].type === 'car');
    
    let safe = [...selectedIndices].sort((a, b) => b - a);
    if (allY.length > 0 && allY.length === selY.length) safe = safe.filter(i => i !== selY[0]);
    
    safe.forEach(i => rects.splice(i, 1));
    selectedIndices = rects.length > 0 ? [rects.length - 1] : [];
    
    updateUI();
    draw();
    canvas.focus();
}



function isHit(r, mx, my)	    
{
    const cos = Math.cos(-r.angle);
    const sin = Math.sin(-r.angle);
    
    const dx  = mx - r.x;
    const dy  = my - r.y;
    const rx  = dx * cos - dy * sin;
    const ry  = dx * sin + dy * cos;
    
    return (rx >= -r.w / 2 && rx <= r.w / 2 && ry >= -r.h / 2 && ry <= r.h / 2);
}



function getLocalCorners(r)	    
{
    const cos = Math.cos(r.angle);
    const sin = Math.sin(r.angle);
    
    const hw = r.w / 2;
    const hh = r.h / 2;
    
    return [{
        x: -hw * cos + hh * sin,
        y: -hw * sin - hh * cos
    }, {
        x: hw * cos + hh * sin,
        y: hw * sin - hh * cos
    }, {
        x: hw * cos - hh * sin,
        y: hw * sin + hh * cos
    }, {
        x: -hw * cos - hh * sin,
        y: -hw * sin + hh * cos
    }];
}



function updateUI()
{
    const allY = rects.reduce((acc, r, i) => (r.type === 'car' ? acc.concat(i) : acc), []);
    const selY = selectedIndices.filter(i => rects[i].type === 'car');
    
    ['Car', 'Spikes', 'Barricade'].forEach(c => {
        const btn  = document.getElementById('btn' + c);
        const type = c.toLowerCase();
	
        if (isShiftDown && selectedIndices.length > 0)
	{
            btn.innerText = `↪ ${c.toLowerCase()}`;
            const protIdx = (allY.length > 0 && allY.length === selY.length && type !== 'car') ? selY[0] : -1;
            btn.disabled  = selectedIndices.filter(idx => idx !== protIdx && rects[idx].type !== type).length === 0;
        }
	else
	{
            btn.innerText = `✚ ${c.toLowerCase()}`;
            btn.disabled  = rects.length >= MAX_RECTS;
        }
    });
    
    const sCount            = selectedIndices.length;
    const sYCount           = selY.length;
    const isDeletingLastCar = (sYCount === allY.length && sCount === sYCount && sYCount === 1);
    
    document.getElementById('deleteBtn').disabled = sCount === 0 || isDeletingLastCar;
}



function draw()	    
{
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    
    if (isShiftDown && selectedIndices.length > 0)
    {
        ctx.beginPath();
        ctx.strokeStyle = '#eee';
	
        for (let x = 0; x <= 700; x += GRID_SIZE)
	{
            ctx.moveTo(x, 0);
            ctx.lineTo(x, 450);
        }
	
        for (let y = 0; y <= 450; y += GRID_SIZE)
	{
            ctx.moveTo(0, y);
            ctx.lineTo(700, y);
        }
	
        ctx.stroke();
    }
    
    const queue = rects.map((r, i) => ({r,i})).sort((a, b) => (LAYER_PRIORITY[a.r.type] - LAYER_PRIORITY[b.r.type]) || (a.r.layer - b.r.layer));
    
    queue.forEach(item => {
        ctx.save();
        ctx.translate(item.r.x, item.r.y);
        ctx.rotate(item.r.angle);
        ctx.drawImage(item.r.img, -item.r.w / 2, -item.r.h / 2, item.r.w, item.r.h);
	
        if (selectedIndices.includes(item.i))
	{
            ctx.strokeStyle = '#007bff';
            ctx.lineWidth   = 3;
            ctx.strokeRect(-item.r.w / 2, -item.r.h / 2, item.r.w, item.r.h);
        }
	
        ctx.restore();
    });
    
    if (isBoxSelecting)
    {
        ctx.strokeStyle = '#007bff';
        ctx.setLineDash([5, 5]);
        ctx.strokeRect(boxStart.x, boxStart.y, boxEnd.x - boxStart.x, boxEnd.y - boxStart.y);
        ctx.setLineDash([]);
    }
    
    let minX = Infinity;
    let maxX = -Infinity;
    let minY = Infinity;
    let maxY = -Infinity;
    
    rects.forEach(r => {
        const corners = getLocalCorners(r);
	
        corners.forEach(p => {
            minX = Math.min(minX, r.x + p.x);
            maxX = Math.max(maxX, r.x + p.x);
            minY = Math.min(minY, r.y + p.y);
            maxY = Math.max(maxY, r.y + p.y);
        });
    });
    
    if (rects.length > 0)
    {
        const mbb = {
            x:  minX,
            y:  minY,
            w:  maxX - minX,
            h:  maxY - minY,
            cx: minX + (maxX - minX) / 2,
            cy: minY + (maxY - minY) / 2
        };
	
        ctx.setLineDash([5, 5]);
        ctx.strokeStyle = '#666';
        ctx.strokeRect(mbb.x, mbb.y, mbb.w, mbb.h);
        ctx.setLineDash([]);
	
        if (rects.length > 1)
	{
            ctx.beginPath();
            ctx.arc(mbb.cx, mbb.cy, 8, 0, Math.PI * 2);
            ctx.fillStyle = 'white';
            ctx.fill();
	    
            ctx.beginPath();
            ctx.arc(mbb.cx, mbb.cy, 5, 0, Math.PI * 2);
            ctx.fillStyle = 'magenta';
            ctx.fill();
        }
	
        updateOutput(mbb);
    }
    
    counterEl.innerText = `Room for ${MAX_RECTS - rects.length} more part(s)`;
}



function updateOutput(mbb)	    
{
    const strPad = (num, places) => String(num).padStart(places, ' ');
    const width  = (mbb.w * LENGTH_SCALE).toFixed(2);
    
    let t           = `minRoadWidth = ${width}\n\n`;
    let firstWidth  = 0;
    let secondWidth = 0;
    
    rects.forEach((r, i) => {
        const ox = ((r.x - mbb.cx) * LENGTH_SCALE).toFixed(2);
        const oy = ((mbb.cy - r.y) * LENGTH_SCALE).toFixed(2);
	
        if (ox.length > firstWidth)  firstWidth = ox.length;
        if (oy.length > secondWidth) secondWidth = oy.length;
    });
    
    rects.forEach((r, i) => {
        const ox = ((r.x - mbb.cx) * LENGTH_SCALE).toFixed(2);
        const oy = ((mbb.cy - r.y) * LENGTH_SCALE).toFixed(2);
	
        const rawDeg = (90 - r.angle * 180 / Math.PI) % 360;
        const disDeg = (r.type === 'car') ? rawDeg : (rawDeg + 90) % 360;
        const ref    = (disDeg < 0) ? (360 + disDeg) : disDeg;
	
        let type = 1;
	
        if (r.type == 'spikes') {
            type = 3;
        } else if (r.type == 'barricade') {
            type = 2;
        }
	
        t += `part0${i+1} = ${type}, ${strPad(ox, firstWidth)}, ${strPad(oy, secondWidth)}, ${(ref / 360.0).toFixed(3)}\n`;
    });
    
    outputField.innerText = t;
}



function clearCanvas()
{
    rects           = [];
    selectedIndices = [];
    addRect('car');
    updateUI();
    draw();
    canvas.focus();
}