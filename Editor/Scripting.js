
// Global parameters ----------------------------------------------------------------------------------------------------------------------------

const IMPORT_REGEX      = /^\s*part(0[1-6])\s*=\s*([1-3])\s*,\s*(-?(?=\d|\.\d)\d*(\.\d*)?)\s*,\s*(-?(?=\d|\.\d)\d*(\.\d*)?)\s*,\s*(-?(?=\d|\.\d)\d*(\.\d*)?)\s*$/;
const IMPORT_PART_REGEX = /^part/i;

const IMPORT_INVALID_COLOUR     = '#D61E00';
const SELECTION_COLOUR          = '#258CF4';
const SELECTION_BOX_FILL_COLOUR = '#258CF40D';
const CONTRAST_COLOUR           = '#EB47EB';

const ANIMATION_TIMER = 2000; // ms

const WIDTH_THRESHOLDS = [6.0, 10.0, 15.0, 22.0, 28.0];

const IMAGE_RENDER_SCALE_BASE = 0.25;
const IMAGE_RENDER_SCALE_MIN  = 0.125;
const IMAGE_RENDER_SCALE_MAX  = 0.35;
const LENGTH_SCALE            = 5.0 / (400 * IMAGE_RENDER_SCALE_BASE);
const IMPORT_ZOOM_MARGIN      = 0.05;

const ZOOM_SLIDER_OFFSET_X            = 12; // px
const ZOOM_SLIDER_OFFSET_Y            = 0;  // px
const ZOOM_SLIDER_LENGTH_FRACTION     = 1.00;
const ZOOM_SLIDER_MARGIN_FRACTION     = 0.05;
const ZOOM_SLIDER_GUIDE_COLOUR        = '#aaa';
const ZOOM_SLIDER_INACCESSIBLE_COLOUR = '#ccc';
const ZOOM_SLIDER_GUIDE_THICKNESS     = 4;  // px
const ZOOM_SLIDER_HANDLE_WIDTH        = 22; // px
const ZOOM_SLIDER_HANDLE_HEIGHT       = 12; // px

const ZOOM_SCROLL_FINE   = (IMAGE_RENDER_SCALE_MAX - IMAGE_RENDER_SCALE_MIN) / 10;
const ZOOM_SCROLL_COARSE = ZOOM_SCROLL_FINE;

const MAX_RECTS = 6;
const GRID_SIZE = 10; // px

const ROT_FINE    = 5 * (Math.PI / 180);
const ROT_COARSE  = 2 * ROT_FINE;
const MOVE_FINE   = 1;
const MOVE_COARSE = 10;

const LAYER_PRIORITY = 
{
	sawhorse: 3,
	car:      2,
	spikes:   1
};

const RECT_CONFIG = 
{
	car:      {src: 'Assets/Images/Car.webp'},
	spikes:   {src: 'Assets/Images/Spikes.webp'},
	sawhorse: {src: 'Assets/Images/Sawhorse.webp'}
};

const TYPE_MAP = 
{
	1: 'car',
	2: 'sawhorse',
	3: 'spikes'
};

const ID_MAP = 
{
	car:      1,
	sawhorse: 2,
	spikes:   3
};





// Global elements ------------------------------------------------------------------------------------------------------------------------------

const canvas    = document.getElementById('mainCanvas');
const workspace = document.getElementById('main');
const context   = canvas.getContext('2d');

const zoomControl = document.getElementById('zoomControl');
const zoomSlider  = document.getElementById('zoomSlider');

const outputField   = document.getElementById('outputField');
const outputOverlay = document.getElementById('outputOverlay');
const canvasOverlay = document.getElementById('canvasOverlay');

const modal           = document.getElementById('importModal');
const importInput     = document.getElementById('importInput');
const importHighlight = document.getElementById('importHighlight');
const btnLoadImport   = document.getElementById('btnLoadImport');
const btnMirror       = document.getElementById('btnMirror');

const typeButtons = 
{
	car:      document.getElementById('btnCar'),
	spikes:   document.getElementById('btnSpikes'),
	sawhorse: document.getElementById('btnSawhorse')
};





// Global state ---------------------------------------------------------------------------------------------------------------------------------

const images = {};

let IMAGE_RENDER_SCALE = IMAGE_RENDER_SCALE_MAX;
let imagesLoaded       = 0;
let viewOffsetX        = 0;
let viewOffsetY        = 0;

let rects           = [];
let selectedIndices = [];

let isDraggingGroup = false;
let isBoxSelecting  = false;

let boxStart = {x: 0, y: 0};
let boxEnd   = {x: 0, y: 0};

let clipboard     = null;
let dragStartData = null;

let isShiftDown      = false;
let isMirrorHover    = false;
let clipboardEnabled = true;

let layerCounter      = 0;
let selectionSnapshot = [];

let cachedCompositionBounds = null;





// Rectangle geometry ----------------------------------------------------------------------------------------------------------------------------

function getLocalCorners(rect, angle = rect.angle)
{
	const cos = Math.cos(angle);
	const sin = Math.sin(angle);
	const hw  = rect.w / 2;
	const hh  = rect.h / 2;

	return [
		{
			x: -hw * cos + hh * sin,
			y: -hw * sin - hh * cos
		}, 
		{
			x: hw * cos + hh * sin,
			y: hw * sin - hh * cos
		}, 
		{
			x: hw * cos - hh * sin,
			y: hw * sin + hh * cos
		}, 
		{
			x: -hw * cos - hh * sin,
			y: -hw * sin + hh * cos
		}
	];
}



function getRectBounds(rect, x = rect.x, y = rect.y, angle = rect.angle)
{
	let minX = Infinity;
	let maxX = -Infinity;
	let minY = Infinity;
	let maxY = -Infinity;

	for (const corner of getLocalCorners(rect, angle))
	{
		minX = Math.min(minX, x + corner.x);
		maxX = Math.max(maxX, x + corner.x);
		minY = Math.min(minY, y + corner.y);
		maxY = Math.max(maxY, y + corner.y);
	}

	return {minX, maxX, minY, maxY};
}



function getCompositionBounds(items = rects)
{
	let minX = Infinity;
	let maxX = -Infinity;
	let minY = Infinity;
	let maxY = -Infinity;

	for (const rect of items)
	{
		const bounds = getRectBounds(rect);

		minX = Math.min(minX, bounds.minX);
		maxX = Math.max(maxX, bounds.maxX);
		minY = Math.min(minY, bounds.minY);
		maxY = Math.max(maxY, bounds.maxY);
	}

	return {minX, maxX, minY, maxY};
}



function canExistAt(rect, x, y, angle)
{
	const bounds = getVisibleBounds();
	const extent = getRectBounds(rect, x, y, angle);

	return (extent.minX >= bounds.left) && (extent.maxX <= bounds.right) && (extent.minY >= bounds.top) && (extent.maxY <= bounds.bottom);
}



function isHit(rect, x, y)
{
	const cos = Math.cos(-rect.angle);
	const sin = Math.sin(-rect.angle);

	const dx = x - rect.x;
	const dy = y - rect.y;
	const rx = dx * cos - dy * sin;
	const ry = dx * sin + dy * cos;

	return (rx >= -rect.w / 2) && (rx <= rect.w / 2) && (ry >= -rect.h / 2) && (ry <= rect.h / 2);
}



function getLayerSortedRects(descending = false)
{
	const direction = descending ? -1 : 1;

	return rects.map((rect, index) => ({rect, index})).sort
	(
		(a, b) =>
		{
			const priority = LAYER_PRIORITY[a.rect.type] - LAYER_PRIORITY[b.rect.type];
			return direction * (priority || a.rect.layer - b.rect.layer);
		}
	);
}





// View and zoom geometry ------------------------------------------------------------------------------------------------------------------------

function getVisibleBounds()
{
	const halfWidth  = canvas.width  / 2;
	const halfHeight = canvas.height / 2;
	const invZoom    = IMAGE_RENDER_SCALE_BASE / IMAGE_RENDER_SCALE;

	return {
		left:   halfWidth  * (1 - invZoom) + viewOffsetX,
		right:  halfWidth  * (1 + invZoom) + viewOffsetX,
		top:    halfHeight * (1 - invZoom) + viewOffsetY,
		bottom: halfHeight * (1 + invZoom) + viewOffsetY
	};
}



function getMaxZoomScale(margin = 0, bounds = getCompositionBounds())
{
	if (rects.length === 0) return IMAGE_RENDER_SCALE_MAX;

	const width  = bounds.maxX - bounds.minX;
	const height = bounds.maxY - bounds.minY;

	const widthScale  = IMAGE_RENDER_SCALE_BASE * canvas.width * (1 - 2 * margin) / width;
	const heightScale = IMAGE_RENDER_SCALE_BASE * canvas.height / height;
	const limit       = Math.min(IMAGE_RENDER_SCALE_MAX, widthScale, heightScale);

	return Math.max(IMAGE_RENDER_SCALE_MIN, limit);
}



function updateZoomControl(bounds = getCompositionBounds())
{
	const maxScale           = getMaxZoomScale(0, bounds);
	const unavailablePercent = 100 * (IMAGE_RENDER_SCALE_MAX - maxScale) / (IMAGE_RENDER_SCALE_MAX - IMAGE_RENDER_SCALE_MIN);

	zoomControl.style.setProperty('--zoom-inaccessible-percent', `${unavailablePercent}%`);

	IMAGE_RENDER_SCALE = Math.min(IMAGE_RENDER_SCALE, maxScale);

	if (rects.length > 0)
	{
		const halfWidth  = canvas.width  * IMAGE_RENDER_SCALE_BASE / (2 * IMAGE_RENDER_SCALE);
		const halfHeight = canvas.height * IMAGE_RENDER_SCALE_BASE / (2 * IMAGE_RENDER_SCALE);

		const minOffsetX = bounds.maxX - halfWidth  - canvas.width / 2;
		const maxOffsetX = bounds.minX + halfWidth  - canvas.width / 2;
		const minOffsetY = bounds.maxY - halfHeight - canvas.height / 2;
		const maxOffsetY = bounds.minY + halfHeight - canvas.height / 2;

		viewOffsetX = (minOffsetX <= maxOffsetX) ? Math.max(minOffsetX, Math.min(viewOffsetX, maxOffsetX)) : (bounds.minX + bounds.maxX - canvas.width)  / 2;
		viewOffsetY = (minOffsetY <= maxOffsetY) ? Math.max(minOffsetY, Math.min(viewOffsetY, maxOffsetY)) : (bounds.minY + bounds.maxY - canvas.height) / 2;
	}

	zoomSlider.value    = IMAGE_RENDER_SCALE;
	zoomSlider.disabled = maxScale <= IMAGE_RENDER_SCALE_MIN;
}



function adjustZoom(delta, coarse)
{
	const step      = (coarse) ? ZOOM_SCROLL_COARSE : ZOOM_SCROLL_FINE;
	const direction = (delta > 0) ? -1 : 1;

	IMAGE_RENDER_SCALE = Math.max(IMAGE_RENDER_SCALE_MIN, Math.min(IMAGE_RENDER_SCALE_MAX, IMAGE_RENDER_SCALE + direction * step));

	draw();
}



function getCanvasPoint(event)
{
	const bounds = canvas.getBoundingClientRect();
	const scale  = IMAGE_RENDER_SCALE / IMAGE_RENDER_SCALE_BASE;
	const cx     = canvas.width  / 2;
	const cy     = canvas.height / 2;

	return {
		x: cx + viewOffsetX + ((event.clientX - bounds.left) * canvas.width  / bounds.width  - cx) / scale,
		y: cy + viewOffsetY + ((event.clientY - bounds.top)  * canvas.height / bounds.height - cy) / scale
	};
}



function snapToPixel(value, centre, scale, offset)
{
	return centre + offset + (Math.floor(centre + (value - centre - offset) * scale) + 0.5 - centre) / scale;
}



function snapToGrid(value, centre, scale, offset)
{
	return centre + offset + (Math.round((centre + (value - centre - offset) * scale) / GRID_SIZE) * GRID_SIZE - centre) / scale;
}



function strokePixelAlignedRect(x, y, width, height, scale)
{
	const cx = canvas.width  / 2;
	const cy = canvas.height / 2;

	const x1 = snapToPixel(x,          cx, scale, viewOffsetX);
	const y1 = snapToPixel(y,          cy, scale, viewOffsetY);
	const x2 = snapToPixel(x + width,  cx, scale, viewOffsetX);
	const y2 = snapToPixel(y + height, cy, scale, viewOffsetY);

	context.strokeRect(x1, y1, x2 - x1, y2 - y1);
}



function strokePixelAlignedSelection(rect, scale)
{
	const cx      = canvas.width  / 2;
	const cy      = canvas.height / 2;
	const corners = getLocalCorners(rect);

	context.beginPath();

	corners.forEach
	(
		(corner, index) =>
		{
			const x = snapToPixel(rect.x + corner.x, cx, scale, viewOffsetX);
			const y = snapToPixel(rect.y + corner.y, cy, scale, viewOffsetY);

			if (index === 0) 
				context.moveTo(x, y);

			else             
				context.lineTo(x, y);
		}
	);

	context.closePath();
	context.stroke();
}





// Rectangle management --------------------------------------------------------------------------------------------------------------------------

function getCarSelection()
{
	const carIndices         = rects.reduce((indices, rect, index) => (rect.type === 'car') ? indices.concat(index) : indices, []);
	const selectedCarIndices = selectedIndices.filter(index => rects[index].type === 'car');

	return {carIndices, selectedCarIndices};
}



function getProtectedCarIndex(type, carSelection)
{
	const {carIndices, selectedCarIndices} = carSelection;

	return ((carIndices.length > 0) && (carIndices.length === selectedCarIndices.length) && (type !== 'car')) ? selectedCarIndices[0] : -1;
}



function addRect(type)
{
	if (rects.length >= MAX_RECTS) return;

	const img = images[type];

	layerCounter++;

	rects.push
	(
		{
			x:     canvas.width  / 2 + viewOffsetX,
			y:     canvas.height / 2 + viewOffsetY,
			w:     img.width  * IMAGE_RENDER_SCALE_BASE,
			h:     img.height * IMAGE_RENDER_SCALE_BASE,
			img,
			type,
			angle: 0,
			layer: layerCounter
		}
	);

	selectedIndices = [rects.length - 1];
}



function transformRect(index, newType)
{
	const rect = rects [index];
	const img  = images[newType];

	rect.type = newType;
	rect.img  = img;
	rect.w    = img.width  * IMAGE_RENDER_SCALE_BASE;
	rect.h    = img.height * IMAGE_RENDER_SCALE_BASE;

	if (!canExistAt(rect, rect.x, rect.y, rect.angle))
	{
		rect.x = canvas.width  / 2 + viewOffsetX;
		rect.y = canvas.height / 2 + viewOffsetY;
	}
}



function handleAction(type)
{
	if (isShiftDown && (selectedIndices.length > 0))
	{
		const protectedIndex = getProtectedCarIndex(type, getCarSelection());

		selectedIndices.forEach
		(	
			index =>
			{
				if (index !== protectedIndex) transformRect(index, type);
			}
		);
	}
	else addRect(type);

	updateUI();
	draw();

	canvas.focus();
}



function deleteSelected()
{
	if (selectedIndices.length === 0) return;

	const {carIndices, selectedCarIndices} = getCarSelection();

	let safeIndices = [...selectedIndices].sort((a, b) => b - a);

	if ((carIndices.length > 0) && (carIndices.length === selectedCarIndices.length))
		safeIndices = safeIndices.filter(index => index !== selectedCarIndices[0]);

	safeIndices.forEach(index => rects.splice(index, 1));
	selectedIndices = rects.length > 0 ? [rects.length - 1] : [];

	updateUI();
	draw();

	canvas.focus();
}



function mirrorRects()
{
	if (rects.length === 0) return;

	const bounds = getCompositionBounds();
	const pivotX = (bounds.minX + bounds.maxX) / 2;

	rects.forEach
	(
		rect =>
		{
			rect.x     = 2 * pivotX - rect.x;
			rect.angle = (rect.type === 'sawhorse' ? 2 * Math.PI : Math.PI) - rect.angle;
		}
	);

	draw();
	canvas.focus();
}



function clearCanvas()
{
	rects           = [];
	selectedIndices = [];
	viewOffsetX     = 0;
	viewOffsetY     = 0;

	addRect('car');

	IMAGE_RENDER_SCALE = getMaxZoomScale(IMPORT_ZOOM_MARGIN);

	updateUI();
	draw();

	canvas.focus();
}





// Import and export -----------------------------------------------------------------------------------------------------------------------------

function parseImportLines(text)
{
	return text.split('\n').map(line => line.trim()).filter(line => IMPORT_PART_REGEX.test(line));
}



function isImportValid(lines)
{
	if (lines.length === 0 || lines.length > MAX_RECTS) return false;

	let hasCar = false;

	for (let index = 0; index < lines.length; index++)
	{
		const match = lines[index].match(IMPORT_REGEX);

		if (!match || (parseInt(match[1], 10) !== index + 1)) return false;

		if (parseInt(match[2]) === 1) hasCar = true;
	}

	return hasCar;
}



function syncImportScroll()
{
	importHighlight.scrollTop  = importInput.scrollTop;
	importHighlight.scrollLeft = importInput.scrollLeft;
}



function updateImportHighlight()
{
	let partIndex = 0;
	let stopped   = false;

	const lines = importInput.value.split('\n');

	const nodes = lines.map
	(
		(line, index) =>
		{
			const span = document.createElement('span');
			const part = line.trim();

			span.textContent = line + (index < lines.length - 1 ? '\n' : '');

			if (IMPORT_PART_REGEX.test(part) && !stopped)
			{
				const match = part.match(IMPORT_REGEX);
	
				if (match && (parseInt(match[1], 10) === partIndex + 1) && (partIndex < MAX_RECTS))
				{
					span.className = 'import-line-valid';
					partIndex++;
				}
				else
				{
					span.className = 'import-line-invalid';
					stopped        = true;
				}
			}

			return span;
		}
	);

	importHighlight.replaceChildren(...nodes);

	syncImportScroll();
}



function openImportModal()
{
	modal.style.display    = 'flex';
	btnLoadImport.disabled = true;
	importInput.value      = '';

	updateImportHighlight();
	importInput.focus();
}



function closeImportModal()
{
	modal.style.display = 'none';
	canvas.focus();
}



function loadImportData()
{
	const lines     = parseImportLines(importInput.value);
	const tempRects = [];

	let localLayerCounter = 0;

	lines.forEach
	(
		line =>
		{
			const match = line.match(IMPORT_REGEX);

			const typeID      = parseInt(match[2]);
			const offsetX     = parseFloat(match[3]);
			const offsetY     = parseFloat(match[5]);
			const orientation = parseFloat(match[7]);

			const type = TYPE_MAP[typeID];
			const img  = images[type];

			const angleDeg = (0.25 - ((type === 'car') ? orientation : orientation - 0.25)) * 360;

			tempRects.push
			(
				{
					x:     offsetX  / LENGTH_SCALE,
					y:     -offsetY / LENGTH_SCALE,
					w:     img.width  * IMAGE_RENDER_SCALE_BASE,
					h:     img.height * IMAGE_RENDER_SCALE_BASE,
					img,
					type,
					angle: angleDeg * (Math.PI / 180),
					layer: ++localLayerCounter
				}
			);
		}
	);

	const importedBounds = getCompositionBounds(tempRects);
	const mbbCx          = (importedBounds.minX + importedBounds.maxX) / 2;
	const mbbCy          = (importedBounds.minY + importedBounds.maxY) / 2;
	const canvasCx       = canvas.width  / 2;
	const canvasCy       = canvas.height / 2;

	rects = tempRects.map
	(
		rect => 
		(
			{
				...rect,
				x: rect.x - mbbCx + canvasCx,
				y: rect.y - mbbCy + canvasCy
			}
		)
	);

	IMAGE_RENDER_SCALE = getMaxZoomScale(IMPORT_ZOOM_MARGIN);
	viewOffsetX        = 0;
	viewOffsetY        = 0;

	updateZoomControl();

	let   isOutOfBounds = false;
	const bounds        = getVisibleBounds();

	rects.forEach(rect =>
	{
		for (const corner of getLocalCorners(rect))
		{
			const x = rect.x + corner.x;
			const y = rect.y + corner.y;

			if ((x < bounds.left) || (x > bounds.right) || (y < bounds.top) || (y > bounds.bottom)) isOutOfBounds = true;
		}
	});

	layerCounter    = localLayerCounter;
	selectedIndices = [];

	closeImportModal();
	updateUI();
	draw();

	const defaultText = 'Left-click to select. Scroll to rotate. Hold Shift to snap.';

	if (isOutOfBounds) canvasOverlay.innerHTML = 'Import <span style="color: #D61E00"><b>out of bounds</b></span>.';
	else               canvasOverlay.innerHTML = 'Import <span style="color: #00A34C"><b>successful</b></span>.';

	setTimeout
	(
		() =>
		{
			canvasOverlay.innerText   = defaultText;
			canvasOverlay.style.color = '#666';
		}, 
		ANIMATION_TIMER
	);
}



function getMaxWidth(minWidth)
{
	for (const maxWidth of WIDTH_THRESHOLDS)
	{
		if (maxWidth > minWidth + 1) return maxWidth;
	}

	return Math.max(minWidth + 5, 60);
}



function updateOutput(mbb)
{
	const strPad = (num, places) => String(num).padStart(places, ' ');

	let firstWidth  = 0;
	let secondWidth = 0;

	const offsets = rects.map
	(
		rect =>
		{
			const x = ((rect.x - mbb.cx) * LENGTH_SCALE).toFixed(2);
			const y = ((mbb.cy - rect.y) * LENGTH_SCALE).toFixed(2);

			firstWidth  = Math.max(firstWidth,  x.length);
			secondWidth = Math.max(secondWidth, y.length);

			return {x, y};
		}
	);

	const minWidth = (mbb.w * LENGTH_SCALE).toFixed(2);
	const maxWidth = getMaxWidth(parseFloat(minWidth));

	let text = `extent = ${minWidth}, ${maxWidth.toFixed(2)}\n\n`;

	rects.forEach
	(
		(rect, index) =>
		{
			const {x, y} = offsets[index];

			let orientation = 90 - rect.angle * 180 / Math.PI;

			if (rect.type !== 'car') orientation += 90;

			orientation %= 360;
			orientation /= 360;

			if (orientation < 0) orientation += 1;

			text += `part0${index + 1} = ${ID_MAP[rect.type]}, ${strPad(x, firstWidth)}, ${strPad(y, secondWidth)}, ${orientation.toFixed(3)}\n`;
		}
	);

	outputField.innerText = text;
}





// Interface state -------------------------------------------------------------------------------------------------------------------------------

function updateUI()
{
	const carSelection = getCarSelection();
	const { carIndices, selectedCarIndices } = carSelection;

	for (const [type, button] of Object.entries(typeButtons))
	{
		if (isShiftDown && (selectedIndices.length > 0))
		{
			button.innerText = `↪ ${type}`;

			const protectedIndex = getProtectedCarIndex(type, carSelection);
			button.disabled = selectedIndices.filter(index => (index !== protectedIndex) && (rects[index].type !== type)).length === 0;
		}
		else
		{
			button.innerText = `✚ ${type}`;
			button.disabled  = rects.length >= MAX_RECTS;
		}
	}

	const selectionCount     = selectedIndices.length;
	const selectedCarCount   = selectedCarIndices.length;
	const isDeletingLastCar = (selectedCarCount === carIndices.length) && (selectionCount === selectedCarCount) && (selectedCarCount === 1);

	document.getElementById('deleteBtn').disabled = (selectionCount === 0) || isDeletingLastCar;
}





// Canvas rendering ------------------------------------------------------------------------------------------------------------------------------


function draw(recalculateGeometry = true)
{
	if (recalculateGeometry)
	{
		cachedCompositionBounds = getCompositionBounds();
		updateZoomControl(cachedCompositionBounds);
	}

	const bounds = cachedCompositionBounds;
	const scale  = IMAGE_RENDER_SCALE / IMAGE_RENDER_SCALE_BASE;

	context.lineWidth = 1;
	context.clearRect(0, 0, canvas.width, canvas.height);

	if (isShiftDown && (selectedIndices.length > 0))
	{
		context.beginPath();
		context.strokeStyle = '#eee';

		for (let x = 0; x <= canvas.width; x += GRID_SIZE)
		{
			context.moveTo(x, 0);
			context.lineTo(x, canvas.height);
		}

		for (let y = 0; y <= canvas.height; y += GRID_SIZE)
		{
			context.moveTo(0, y);
			context.lineTo(canvas.width, y);
		}

		context.stroke();
	}

	context.save();

	context.translate(canvas.width / 2, canvas.height / 2);
	context.scale(scale, scale);

	context.translate(-canvas.width / 2 - viewOffsetX, -canvas.height / 2 - viewOffsetY);
	context.lineWidth = 1 / scale;

	let mbb = null;

	if (rects.length > 0)
	{
		const {minX, maxX, minY, maxY} = bounds;

		mbb = 
		{
			x:  Math.floor(minX) + 0.5,
			y:  Math.floor(minY) + 0.5,
			w:  Math.floor(maxX - minX + 1e-9),
			h:  Math.floor(maxY - minY + 1e-9),
			cx: rects.length === 1 ? rects[0].x : minX + (maxX - minX) / 2,
			cy: rects.length === 1 ? rects[0].y : minY + (maxY - minY) / 2
		};

		context.setLineDash([5 / scale, 5 / scale]);
		context.strokeStyle = '#BBB';

		strokePixelAlignedRect(mbb.x, mbb.y, mbb.w, mbb.h, scale);
		context.setLineDash([]);

		if (recalculateGeometry) updateOutput(mbb);
	}

	const queue = getLayerSortedRects();

	queue.forEach
	(
		({rect, index}) =>
		{
			context.save();

			context.translate(rect.x, rect.y);
			context.rotate(rect.angle);

			context.drawImage(rect.img, -rect.w / 2, -rect.h / 2, rect.w, rect.h);
			context.restore();

			if (selectedIndices.includes(index))
			{
				context.save();

				context.strokeStyle = SELECTION_COLOUR;
				context.lineWidth   = 3 / scale;

				strokePixelAlignedSelection(rect, scale);
				context.restore();
			}
		}
	);

	if (isBoxSelecting)
	{
		const x      = Math.floor(boxStart.x) + 0.5;
		const y      = Math.floor(boxStart.y) + 0.5;
		const width  = Math.floor(boxEnd.x - boxStart.x);
		const height = Math.floor(boxEnd.y - boxStart.y);

		context.fillStyle   = SELECTION_BOX_FILL_COLOUR;
		context.fillRect(x, y, width, height);
		context.strokeStyle = SELECTION_COLOUR;

		strokePixelAlignedRect(x, y, width, height, scale);

		context.setLineDash([]);
	}

	if (rects.length > 1)
	{
		const pointX      = snapToPixel(mbb.cx,      canvas.width  / 2, scale, viewOffsetX);
		const pointY      = snapToPixel(mbb.cy,      canvas.height / 2, scale, viewOffsetY);
		const lineTopY    = snapToPixel(bounds.minY, canvas.height / 2, scale, viewOffsetY) - 0.5 / scale;
		const lineBottomY = snapToPixel(bounds.maxY, canvas.height / 2, scale, viewOffsetY) - 0.5 / scale;

		context.beginPath();
		context.arc(pointX, pointY, 8 / scale, 0, Math.PI * 2);
		context.fillStyle = 'white';
		context.fill();

		if (isMirrorHover)
		{
			context.beginPath();
			context.moveTo(pointX, lineTopY);
			context.lineTo(pointX, lineBottomY);

			context.strokeStyle = 'white';
			context.lineWidth   = 7 / scale;

			context.stroke();
		}

		context.beginPath();
		context.arc(pointX, pointY, 5 / scale, 0, Math.PI * 2);
		context.fillStyle = CONTRAST_COLOUR;
		context.fill();

		if (isMirrorHover)
		{
			context.beginPath();
			context.moveTo(pointX, lineTopY);
			context.lineTo(pointX, lineBottomY);

			context.strokeStyle = CONTRAST_COLOUR;
			context.lineWidth   = 1 / scale;

			context.stroke();
		}
	}

	context.restore();
}





// Keyboard interactions -------------------------------------------------------------------------------------------------------------------------

function moveSelectedWithArrow(key)
{
	const step   = (isShiftDown) ? MOVE_COARSE : MOVE_FINE;
	const bounds = getVisibleBounds();

	let limit = step;

	for (const index of selectedIndices)
	{
		const extent = getRectBounds(rects[index]);

		if (key === 'ArrowUp')    limit = Math.min(limit, extent.minY   - bounds.top);
		if (key === 'ArrowDown')  limit = Math.min(limit, bounds.bottom - extent.maxY);
		if (key === 'ArrowLeft')  limit = Math.min(limit, extent.minX   - bounds.left);
		if (key === 'ArrowRight') limit = Math.min(limit, bounds.right  - extent.maxX);
	}

	if (limit <= 0) return;

	selectedIndices.forEach
	(
		index =>
		{
			if (key === 'ArrowUp')    rects[index].y -= limit;
			if (key === 'ArrowDown')  rects[index].y += limit;
			if (key === 'ArrowLeft')  rects[index].x -= limit;
			if (key === 'ArrowRight') rects[index].x += limit;
		}
	);

	draw();
}



function pasteClipboard()
{
	if (rects.length + clipboard.length > MAX_RECTS) return;

	const nextSelection = [];

	clipboard.forEach
	(
		copied =>
		{
			layerCounter++;

			const rect = 
			{
				...copied,
				x:     copied.x + 15,
				y:     copied.y + 15,
				layer: layerCounter
			};

			if (!canExistAt(rect, rect.x, rect.y, rect.angle))
			{
				rect.x = canvas.width  / 2 + viewOffsetX;
				rect.y = canvas.height / 2 + viewOffsetY;
			}

			rects.push(rect);
			nextSelection.push(rects.length - 1);
		}
	);

	selectedIndices = nextSelection;

	updateUI();
	draw();
}



function onKeyDown(event)
{
	if (event.key === 'Escape')
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
	const key            = event.key.toLowerCase();

	if ((key === 'm') && isCanvasActive)
	{
		event.preventDefault();
		mirrorRects();
		return;
	}

	if (event.key === 'Shift')
	{
		isShiftDown = true;
		updateUI();
		draw();
	}

	if ((event.key === 'Delete') || (event.key === 'Backspace')) deleteSelected();

	if (event.ctrlKey && (key === 'a') && isCanvasActive)
	{
		event.preventDefault();
		selectedIndices = rects.map((_, index) => index);
		updateUI();
		draw();
	}

	if (event.ctrlKey && (key === 'c') && (selectedIndices.length > 0))
		clipboard = selectedIndices.map(index => ({...rects[index]}));

	if (event.ctrlKey && (key === 'v') && clipboard) pasteClipboard();

	if (['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(event.key) && (selectedIndices.length > 0) && isCanvasActive)
	{
		event.preventDefault();
		moveSelectedWithArrow(event.key);
	}
}



function onKeyUp(event)
{
	if (event.key === 'Shift')
	{
		isShiftDown = false;
		updateUI();
		draw();
	}
}





// Pointer interactions --------------------------------------------------------------------------------------------------------------------------

function onCanvasMouseDown(event)
{
	canvas.focus();

	const {x, y} = getCanvasPoint(event);

	let hitIndex = -1;

	for (const {rect, index} of getLayerSortedRects(/* descending = */true))
	{
		if (isHit(rect, x, y))
		{
			hitIndex = index;
			break;
		}
	}

	if (hitIndex !== -1)
	{
		if (isShiftDown && selectedIndices.includes(hitIndex))
		{
			selectedIndices = selectedIndices.filter(index => index !== hitIndex);

			isDraggingGroup = false;
			dragStartData   = null;

			updateUI();
			draw();

			return;
		}

		if (!selectedIndices.includes(hitIndex))
		{
			if (!isShiftDown) selectedIndices = [hitIndex];
			else              selectedIndices.push(hitIndex);
		}
		
		layerCounter++;

		rects[hitIndex].layer = layerCounter;
		isDraggingGroup       = true;

		const minX = Math.min(...selectedIndices.map(index => rects[index].x));
		const minY = Math.min(...selectedIndices.map(index => rects[index].y));

		dragStartData = 
		{
			groupMinX:    minX,
			groupMinY:    minY,
			mouseOffsetX: x - minX,
			mouseOffsetY: y - minY,
			itemStarts:   selectedIndices.map
			(
				index => 
				(
					{
						idx: index,
						x: rects[index].x,
						y: rects[index].y
					}
				)
			)
		};
	}
	else
	{
		if (!isShiftDown) selectedIndices = [];

		selectionSnapshot = [...selectedIndices];
		isBoxSelecting    = true;
		boxStart          = {x, y};
		boxEnd            = {x, y};
	}

	updateUI();
	draw();
}



function dragSelectedGroup(x, y)
{
	let targetX = x - dragStartData.mouseOffsetX;
	let targetY = y - dragStartData.mouseOffsetY;

	if (isShiftDown)
	{
		const zoom = IMAGE_RENDER_SCALE / IMAGE_RENDER_SCALE_BASE;
		const cx   = canvas.width  / 2;
		const cy   = canvas.height / 2;

		targetX = snapToGrid(targetX, cx, zoom, viewOffsetX);
		targetY = snapToGrid(targetY, cy, zoom, viewOffsetY);
	}

	const dx = targetX - dragStartData.groupMinX;
	const dy = targetY - dragStartData.groupMinY;

	const positions = dragStartData.itemStarts.map
	(
		start => 
		(
			{
				idx: start.idx,
				x:   start.x + dx,
				y:   start.y + dy
			}
		)
	);

	let shiftLeft  = 0;
	let shiftRight = 0;
	let shiftUp    = 0;
	let shiftDown  = 0;

	const bounds = getVisibleBounds();

	positions.forEach
	(
		position =>
		{
			const extent = getRectBounds(rects[position.idx], position.x, position.y);

			if (extent.minX < bounds.left)   shiftRight = Math.max(shiftRight, bounds.left   - extent.minX);
			if (extent.maxX > bounds.right)  shiftLeft  = Math.min(shiftLeft,  bounds.right  - extent.maxX);
			if (extent.minY < bounds.top)    shiftDown  = Math.max(shiftDown,  bounds.top    - extent.minY);
			if (extent.maxY > bounds.bottom) shiftUp    = Math.min(shiftUp,    bounds.bottom - extent.maxY);
		}
	);

	positions.forEach
	(	
		position =>
		{
			rects[position.idx].x = position.x + shiftRight + shiftLeft;
			rects[position.idx].y = position.y + shiftDown  + shiftUp;
		}
	);
}



function updateBoxSelection(x, y)
{
	boxEnd = {x, y};

	const xMin = Math.min(boxStart.x, boxEnd.x);
	const xMax = Math.max(boxStart.x, boxEnd.x);
	const yMin = Math.min(boxStart.y, boxEnd.y);
	const yMax = Math.max(boxStart.y, boxEnd.y);

	const inBox = rects.map
	(
		(rect, index) => 
		(
			((rect.x >= xMin) && (rect.x <= xMax) && (rect.y >= yMin) && (rect.y <= yMax)) ? index : -1
		)
	).filter(index => (index !== -1));

	let nextSelection;

	if (isShiftDown)
		nextSelection = selectionSnapshot.filter(index => !inBox.includes(index)).concat(inBox.filter(index => !selectionSnapshot.includes(index)));

	else 
		nextSelection = Array.from(new Set([...selectionSnapshot, ...inBox]));

	const selectionChanged = ((nextSelection.length !== selectedIndices.length) || nextSelection.some((index, position) => (index !== selectedIndices[position])));

	if (selectionChanged)
	{
		selectedIndices = nextSelection;

		updateUI();
	}
}



function onMouseMove(event)
{
	if (!isDraggingGroup && !isBoxSelecting) return;

	const {x, y} = getCanvasPoint(event);

	if (isDraggingGroup && dragStartData) 
		dragSelectedGroup(x, y);

	else if (isBoxSelecting)
		updateBoxSelection(x, y);

	draw(!isBoxSelecting);
}



function onMouseUp()
{
	isDraggingGroup = false;
	isBoxSelecting  = false;

	draw();
}



function onWindowWheel(event)
{
	const bounds     = canvas.getBoundingClientRect();
	const overCanvas = (event.clientX >= bounds.left) && (event.clientX <= bounds.right) && (event.clientY >= bounds.top) && (event.clientY <= bounds.bottom);

	if (!overCanvas && (selectedIndices.length === 0)) return;
	if (zoomControl.contains(event.target))            return;
	if (modal.style.display === 'flex')                return;

	const delta = (event.deltaY !== 0) ? event.deltaY : event.deltaX;
	if (delta === 0) return;

	if (event.ctrlKey)
	{
		event.preventDefault();

		if (zoomSlider.disabled) return;

		adjustZoom(delta, event.shiftKey || isShiftDown);

		return;
	}

	if (selectedIndices.length > 0)
	{
		event.preventDefault();

		const step      = (event.shiftKey || isShiftDown) ? ROT_COARSE : ROT_FINE;
		const direction = delta > 0 ? 1 : -1;

		const allRectsMayRotate = selectedIndices.every
		(
			index =>
			{
				const rect = rects[index];
				return canExistAt(rect, rect.x, rect.y, rect.angle + direction * step);
			}
		);

		if (allRectsMayRotate)
		{
			selectedIndices.forEach
			(
				index =>
				{
					rects[index].angle += direction * step;
				}
			);

			draw();
		}
	}
}





// Initialisation and listeners ------------------------------------------------------------------------------------------------------------------

function initDefault()
{
	document.getElementById('error').style.display = 'none';
	workspace.style.display                        = 'flex';

	addRect('car');

	draw();
	canvas.focus();
}



const zoomTrackLength = canvas.height * Math.min(ZOOM_SLIDER_LENGTH_FRACTION, 1 - 2 * ZOOM_SLIDER_MARGIN_FRACTION);

workspace.style.setProperty('--zoom-offset-x',                  `${ZOOM_SLIDER_OFFSET_X}px`);
workspace.style.setProperty('--zoom-offset-y',                  `${ZOOM_SLIDER_OFFSET_Y + (canvas.height - zoomTrackLength) / 2}px`);
workspace.style.setProperty('--zoom-track-length',              `${zoomTrackLength}px`);
workspace.style.setProperty('--zoom-guide-colour',              ZOOM_SLIDER_GUIDE_COLOUR);
workspace.style.setProperty('--zoom-guide-inaccessible-colour', ZOOM_SLIDER_INACCESSIBLE_COLOUR);
workspace.style.setProperty('--zoom-guide-thickness',           `${ZOOM_SLIDER_GUIDE_THICKNESS}px`);
workspace.style.setProperty('--zoom-handle-width',              `${ZOOM_SLIDER_HANDLE_WIDTH}px`);
workspace.style.setProperty('--zoom-handle-height',             `${ZOOM_SLIDER_HANDLE_HEIGHT}px`);

importHighlight.style.setProperty('--import-invalid-colour', IMPORT_INVALID_COLOUR);

zoomSlider.min   = IMAGE_RENDER_SCALE_MIN;
zoomSlider.max   = IMAGE_RENDER_SCALE_MAX;
zoomSlider.value = IMAGE_RENDER_SCALE;



zoomSlider.addEventListener
(
	'input', () =>
	{
		IMAGE_RENDER_SCALE = Number(zoomSlider.value);
		draw();
	}
);



zoomControl.addEventListener('mousedown', event => event.stopPropagation());



zoomControl.addEventListener
(
	'wheel', event =>
	{
		if (zoomSlider.disabled) return;

		const delta = (event.deltaY !== 0) ? event.deltaY : event.deltaX;
		if (delta === 0) return;

		event.preventDefault();
		adjustZoom(delta, event.shiftKey || isShiftDown);
	}, 
	{passive: false}
);



modal.addEventListener
(
	'mousedown', event =>
	{
		if (event.target === modal) closeImportModal();
	}
);



importInput.addEventListener
(
	'input', () =>
	{
		btnLoadImport.disabled = !isImportValid(parseImportLines(importInput.value));
		updateImportHighlight();
	}
);



importInput.addEventListener('scroll', syncImportScroll);



outputField.addEventListener
(
	'click', () =>
	{
		if (!clipboardEnabled) return;

		navigator.clipboard.writeText(outputField.innerText).then
		(
			() =>
			{
				outputOverlay.innerHTML = '<span style="color: #DEB700"><b>Copied</b></span> settings.';
				outputField.classList.remove('flash');

				clipboardEnabled = false;

				setTimeout
				(
					() =>
					{
						outputOverlay.innerHTML = '<span class="emph">Click to copy</span> settings.';
						outputField.classList.add('flash');

						clipboardEnabled = true;
					}, 
					ANIMATION_TIMER
				);
			}
		);
	}
);



document.addEventListener
(
	'mousedown', event =>
	{
		if (event.target         === canvas)      return;
		if (event.target.tagName === 'BUTTON')    return;
		if (event.target         === outputField) return;

		if (modal.contains(event.target)) return;
		if (event.target === modal)       return;

		selectedIndices = [];
		updateUI();
		draw();
	}
);



window.addEventListener('keydown', onKeyDown);
window.addEventListener('keyup',   onKeyUp);

canvas.addEventListener('mousedown', onCanvasMouseDown);
window.addEventListener('mousemove', onMouseMove);
window.addEventListener('mouseup',   onMouseUp);

window.addEventListener('wheel', onWindowWheel, {passive: false});



btnMirror.addEventListener
(
	'mouseenter', () =>
	{
		isMirrorHover = true;
		draw();
	}
);



btnMirror.addEventListener
(
	'mouseleave', () =>
	{
		isMirrorHover = false;
		draw();
	}
);



for (const [type, config] of Object.entries(RECT_CONFIG))
{
	const image = new Image();

	image.onload = () =>
	{
		if (++imagesLoaded === Object.keys(RECT_CONFIG).length) initDefault();
	};

	image.src    = config.src;
	images[type] = image;
}