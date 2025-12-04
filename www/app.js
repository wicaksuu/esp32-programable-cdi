// Global variables
let currentMapIndex = 0;
let allMaps = [];
let ignitionChart = null;
let calibrationInterval = null;
let currentIgnitionPoints = [];  // Dynamic ignition points
let currentQSPoints = [];  // Dynamic QS points
// Toast notification system
function showToast(message, type = 'info', duration = 3000) {
const container = document.getElementById('toast-container') || createToastContainer();
const toast = document.createElement('div');
toast.className = `toast toast-${type}`;
const icon = {
'success': '✓',
'error': '✕',
'warning': '⚠',
'info': 'ℹ'
}[type] || 'ℹ';
toast.innerHTML = `
<span class="toast-icon">${icon}</span>
<span class="toast-message">${message}</span>
`;
container.appendChild(toast);
setTimeout(() => toast.classList.add('show'), 10);
setTimeout(() => {
toast.classList.remove('show');
setTimeout(() => toast.remove(), 300);
}, duration);
}
function createToastContainer() {
const container = document.createElement('div');
container.id = 'toast-container';
document.body.appendChild(container);
return container;
}
// Confirmation modal system
function showConfirm(message, title = 'Confirm') {
return new Promise((resolve) => {
const overlay = document.createElement('div');
overlay.className = 'confirm-overlay';
const modal = document.createElement('div');
modal.className = 'confirm-modal';
modal.innerHTML = `
<div class="confirm-header">
<h3>${title}</h3>
</div>
<div class="confirm-body">
<p>${message}</p>
</div>
<div class="confirm-footer">
<button class="confirm-btn confirm-btn-cancel">Cancel</button>
<button class="confirm-btn confirm-btn-confirm">Confirm</button>
</div>
`;
overlay.appendChild(modal);
document.body.appendChild(overlay);
setTimeout(() => overlay.classList.add('show'), 10);
const handleClose = (result) => {
overlay.classList.remove('show');
setTimeout(() => overlay.remove(), 300);
resolve(result);
};
modal.querySelector('.confirm-btn-cancel').onclick = () => handleClose(false);
modal.querySelector('.confirm-btn-confirm').onclick = () => handleClose(true);
overlay.onclick = (e) => {
if (e.target === overlay) handleClose(false);
};
});
}
// Initialize on page load
document.addEventListener('DOMContentLoaded', function() {
initTabs();
loadAllMaps();
updateStatus();
// Auto-refresh status every 1 second
setInterval(updateStatus, 1000);
// Auto-load active map for editing (after maps are loaded)
setTimeout(() => {
autoLoadActiveMap();
autoLoadActiveMapForLC();
autoLoadActiveMapForAWTC();
}, 500);
});
// Tab navigation
function initTabs() {
const tabBtns = document.querySelectorAll('.tab-btn');
const tabContents = document.querySelectorAll('.tab-content');
tabBtns.forEach(btn => {
btn.addEventListener('click', () => {
const targetTab = btn.dataset.tab;
tabBtns.forEach(b => b.classList.remove('active'));
tabContents.forEach(c => c.classList.remove('active'));
btn.classList.add('active');
document.getElementById(targetTab + '-tab').classList.add('active');
});
});
}
// Refresh maps data only (without UI updates or changing currentMapIndex)
async function refreshAllMapsData() {
try {
const response = await fetch('/api/maps');
const data = await response.json();
allMaps = data.maps;
} catch (error) {
}
}
// Load all mappings
async function loadAllMaps() {
try {
const response = await fetch('/api/maps');
const data = await response.json();
allMaps = data.maps;
displayMaps();
populateMapSelects();
// Find and set active map index
const activeMapIndex = allMaps.findIndex(map => map.isActive);
if (activeMapIndex >= 0) {
currentMapIndex = activeMapIndex;
}
} catch (error) {
showToast('Failed to load mappings', 'error');
}
}
// Auto-load active map for editing
async function autoLoadActiveMap() {
if (allMaps.length === 0) return;
// Find active map
const activeMapIndex = allMaps.findIndex(map => map.isActive);
if (activeMapIndex >= 0) {
currentMapIndex = activeMapIndex;
document.getElementById('edit-map-select').value = activeMapIndex;
await loadMapForEdit();
// Show indicator that this is the active map
}
}
// Auto-load active map for LC tab
async function autoLoadActiveMapForLC() {
if (allMaps.length === 0) return;
// Find active map
const activeMapIndex = allMaps.findIndex(map => map.isActive);
if (activeMapIndex >= 0) {
currentMapIndex = activeMapIndex;
const lcSelect = document.getElementById('lc-edit-map-select');
if (lcSelect) {
lcSelect.value = activeMapIndex;
await loadMapForLCEdit();
}
}
}
// Auto-load active map for AWTC editing
async function autoLoadActiveMapForAWTC() {
if (allMaps.length === 0) return;
// Find active map
const activeMapIndex = allMaps.findIndex(map => map.isActive);
if (activeMapIndex >= 0) {
currentMapIndex = activeMapIndex;
const awtcSelect = document.getElementById('awtc-edit-map-select');
if (awtcSelect) {
awtcSelect.value = activeMapIndex;
await loadMapForAWTCEdit();
}
}
}
// Display maps in grid
function displayMaps() {
const mapsList = document.getElementById('maps-list');
mapsList.innerHTML = '';
allMaps.forEach((map, index) => {
const mapCard = document.createElement('div');
mapCard.className = 'map-card' + (map.isActive ? ' active' : '');
// Build feature badges
const features = [];
if (map.lcEnabled) features.push('<span class="feature-badge lc">🚀 LC</span>');
if (map.qsEnabled) features.push('<span class="feature-badge qs">⚡ QS</span>');
if (map.awEnabled) features.push('<span class="feature-badge aw">🛡️ AW</span>');
if (map.tcEnabled) features.push('<span class="feature-badge tc">🔒 TC</span>');
const featuresHTML = features.length > 0 ? features.join('') : '<span class="feature-badge none">No Features</span>';
mapCard.innerHTML = `
<div class="map-card-header">
<h3>${map.name}</h3>
<span class="map-status-badge ${map.isActive ? 'active' : ''}">${map.isActive ? '✓ Active' : 'Inactive'}</span>
</div>
<div class="map-card-info">
<div class="map-info-item">
<span class="map-info-label">RPM Range:</span>
<span class="map-info-value">${map.minRPM || 0} - ${map.maxRPM || 0}</span>
</div>
<div class="map-info-item">
<span class="map-info-label">Rev Limiter:</span>
<span class="map-info-value">${map.revLimiterEnabled ? map.revLimiter + ' RPM' : 'OFF'}</span>
</div>
<div class="map-info-item">
<span class="map-info-label">Ignition Points:</span>
<span class="map-info-value">${map.ignitionPointCount || 0}</span>
</div>
<div class="map-info-item">
<span class="map-info-label">Dwell Time:</span>
<span class="map-info-value">${(map.dwellTimeUS || 0) / 1000} ms</span>
</div>
</div>
<div class="map-card-features">
<div class="features-label">Active Features:</div>
<div class="features-badges">${featuresHTML}</div>
</div>
<div class="map-card-actions">
<button class="btn btn-primary btn-small" onclick="selectMap(${index})" title="Activate this map">
${map.isActive ? '✓ Active' : '⚡ Activate'}
</button>
<button class="btn btn-secondary btn-small" onclick="editMap(${index})" title="Edit map">
✏️ Edit
</button>
<button class="btn btn-info btn-small" onclick="duplicateMap(${map.id})" title="Duplicate map">
📋 Copy
</button>
${!map.isActive ? `<button class="btn btn-danger btn-small" onclick="deleteMap(${map.id})" title="Delete map">🗑️ Delete</button>` : ''}
</div>
`;
mapsList.appendChild(mapCard);
});
}
// Populate map select dropdowns
function populateMapSelects() {
const editSelect = document.getElementById('edit-map-select');
const copySelect = document.getElementById('copy-from-map');
const lcEditSelect = document.getElementById('lc-edit-map-select');
const awtcEditSelect = document.getElementById('awtc-edit-map-select');
editSelect.innerHTML = '';
copySelect.innerHTML = '<option value="-1">Start from default</option>';
lcEditSelect.innerHTML = '';
awtcEditSelect.innerHTML = '';
allMaps.forEach((map, index) => {
const option1 = new Option(map.name, index);
const option2 = new Option(map.name, index);
const option3 = new Option(`${map.name}${map.isActive ? ' (Active)' : ''}`, index);
const option4 = new Option(`${map.name}${map.isActive ? ' (Active)' : ''}`, index);
if (map.isActive) {
option1.selected = true;
option3.selected = true;
option4.selected = true;
}
editSelect.add(option1);
copySelect.add(option2);
lcEditSelect.add(option3);
awtcEditSelect.add(option4);
});
}
// Select active map
async function selectMap(index) {
try {
const formData = new FormData();
formData.append('index', index);
const response = await fetch('/api/selectMap', {
method: 'POST',
body: formData
});
if (response.ok) {
currentMapIndex = index;
await loadAllMaps();
// Sync all selectors and load data after activating
await syncAllMapSelectors(index);
showToast('Map activated: ' + allMaps[index].name);
}
} catch (error) {
showToast('Failed to select map', 'error');
}
}
// Edit map
async function editMap(index) {
await syncAllMapSelectors(index);
document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
document.querySelector('[data-tab="ignition"]').classList.add('active');
document.getElementById('ignition-tab').classList.add('active');
}
// Create new map
function createNewMap() {
document.getElementById('modal-title').textContent = 'Create New Map';
document.getElementById('new-map-name').value = '';
document.getElementById('modal').style.display = 'block';
}
function closeModal() {
document.getElementById('modal').style.display = 'none';
}
// OLD FUNCTION - Not used anymore, kept for compatibility
// Use createNewMap() or duplicateMap() instead
async function confirmCreateMap() {
const mapName = document.getElementById('new-map-name')?.value;
const copyFromIndex = parseInt(document.getElementById('copy-from-map')?.value);
if (!mapName) {
showToast('Please enter a map name', 'warning');
return;
}
try {
if (copyFromIndex >= 0) {
// Use duplicate API
await duplicateMap(copyFromIndex);
} else {
// Use create API (creates 8-point default template)
const response = await fetch('/api/maps/create', {
method: 'POST',
headers: { 'Content-Type': 'application/json' },
body: JSON.stringify({ name: mapName })
});
const result = await response.json();
if (result.success) {
showToast('Map created successfully', 'success');
closeModal();
await loadAllMaps();
} else {
showToast('Failed to create map: ' + (result.error || 'Unknown error'));
}
}
} catch (error) {
showToast('Failed to create map', 'error');
}
}
// Sync all map selectors and load data for all tabs
async function syncAllMapSelectors(targetIndex) {
if (targetIndex === undefined || targetIndex === null) return;
const desiredIndex = parseInt(targetIndex);
currentMapIndex = desiredIndex;
await refreshAllMapsData();
currentMapIndex = desiredIndex;
const editSelect = document.getElementById('edit-map-select');
const lcSelect = document.getElementById('lc-edit-map-select');
const awtcSelect = document.getElementById('awtc-edit-map-select');
if (editSelect) editSelect.value = desiredIndex;
if (lcSelect) lcSelect.value = desiredIndex;
if (awtcSelect) awtcSelect.value = desiredIndex;
await loadMapDataOnly(desiredIndex);
}
// Load map data without refreshing allMaps (for internal use by syncAllMapSelectors)
async function loadMapDataOnly(index) {
try {
const response = await fetch(`/api/maps/${index}`);
const map = await response.json();
const isActive = allMaps[index]?.isActive || false;
loadIgnitionData(map, isActive);
loadLCData(map, isActive);
loadAWTCData(map, isActive);
} catch (error) {
}
}
// Load Ignition & Settings data
function loadIgnitionData(map, isActive) {
// Show active/inactive notice for Ignition tab
document.getElementById('active-map-notice').style.display = isActive ? 'block' : 'none';
document.getElementById('inactive-map-notice').style.display = isActive ? 'none' : 'block';
// Show active/inactive notice for QS tab
document.getElementById('qs-active-map-notice').style.display = isActive ? 'block' : 'none';
document.getElementById('qs-inactive-map-notice').style.display = isActive ? 'none' : 'block';
// Show active/inactive notice for Settings tab
document.getElementById('settings-active-map-notice').style.display = isActive ? 'block' : 'none';
document.getElementById('settings-inactive-map-notice').style.display = isActive ? 'none' : 'block';
// Populate ignition settings
document.getElementById('engine-type').value = map.engineType || 0;
document.getElementById('pickup-sensor-offset').value = map.pickupSensorOffset || 0;
document.getElementById('dwell-time').value = map.dwellTimeUS;
document.getElementById('rev-limiter').value = map.revLimiterRPM;
document.getElementById('rev-limiter-enabled').checked = map.revLimiterEnabled;
document.getElementById('rev-limiter-cut-pattern').value = map.revLimiterCutPattern !== undefined ? map.revLimiterCutPattern : 2;
// Populate ignition curve table (dynamic points)
currentIgnitionPoints = [];
const ignitionPointCount = map.ignitionPointCount || map.ignitionRPM.length;
for (let i = 0; i < ignitionPointCount; i++) {
currentIgnitionPoints.push({
rpm: map.ignitionRPM[i],
advance: map.ignitionAdvance[i]
});
}
generateIgnitionTable();
// Draw curve
drawIgnitionCurve(map.ignitionRPM.slice(0, ignitionPointCount), map.ignitionAdvance.slice(0, ignitionPointCount));
// Populate quick shifter settings
document.getElementById('qs-enabled').checked = map.qsEnabled;
document.getElementById('qs-sensor-threshold').value = map.qsSensorThreshold;
document.getElementById('qs-sensor-type').value = map.qsSensorInvert ? 'Inverted' : 'Normal';
document.getElementById('qs-min-rpm').value = map.qsMinRPM;
document.getElementById('qs-max-rpm').value = map.qsMaxRPM;
// Populate QS kill time table (dynamic points)
currentQSPoints = [];
const qsPointCount = map.qsPointCount || map.qsRPM.length;
for (let i = 0; i < qsPointCount; i++) {
currentQSPoints.push({
rpm: map.qsRPM[i],
killTime: map.qsKillTime[i]
});
}
generateQSTable();
// Populate settings tab
document.getElementById('map-name').value = map.name;
// Input mode
const acModeRadio = document.querySelector('input[name="input-mode"][value="ac"]');
const dcModeRadio = document.querySelector('input[name="input-mode"][value="dc"]');
if (map.isACMode) {
acModeRadio.checked = true;
} else {
dcModeRadio.checked = true;
}
// AC mode settings
document.getElementById('ac-threshold').value = map.acTriggerThreshold;
document.getElementById('ac-invert').checked = map.acInvertSignal;
// DC mode settings
document.getElementById('dc-pulses').value = map.dcPulsesPerRev;
document.getElementById('dc-pullup').checked = map.dcPullupEnabled;
}
// Load LC data
function loadLCData(map, isActive) {
document.getElementById('lc-enabled').checked = map.lcEnabled || false;
document.getElementById('lc-target-rpm').value = map.lcTargetRPM || 6000;
document.getElementById('lc-retard-degrees').value = map.lcRetardDegrees !== undefined ? map.lcRetardDegrees : 10;
document.getElementById('lc-cut-pattern').value = map.lcCutPattern !== undefined ? map.lcCutPattern : 2;
document.getElementById('lc-active-map-notice').style.display = isActive ? 'block' : 'none';
document.getElementById('lc-inactive-map-notice').style.display = isActive ? 'none' : 'block';
}
// Load AWTC data
function loadAWTCData(map, isActive) {
document.getElementById('aw-enabled').checked = map.awEnabled || false;
document.getElementById('aw-pitch-threshold').value = map.awPitchThreshold !== undefined ? map.awPitchThreshold : 15.0;
document.getElementById('aw-cut-pattern').value = map.awCutPattern !== undefined ? map.awCutPattern : 2;
document.getElementById('aw-retard-degrees').value = map.awRetardDegrees !== undefined ? map.awRetardDegrees : 5;
document.getElementById('tc-enabled').checked = map.tcEnabled || false;
document.getElementById('tc-front-wheel-holes').value = map.tcFrontWheelHoles || 4;
document.getElementById('tc-rear-wheel-holes').value = map.tcRearWheelHoles || 4;
document.getElementById('tc-slip-threshold').value = map.tcSlipThreshold !== undefined ? map.tcSlipThreshold : 0.15;
document.getElementById('tc-cut-pattern').value = map.tcCutPattern !== undefined ? map.tcCutPattern : 2;
document.getElementById('tc-retard-degrees').value = map.tcRetardDegrees !== undefined ? map.tcRetardDegrees : 5;
document.getElementById('awtc-active-map-notice').style.display = isActive ? 'block' : 'none';
document.getElementById('awtc-inactive-map-notice').style.display = isActive ? 'none' : 'block';
}
// Load map for editing
async function loadMapForEdit() {
const index = parseInt(document.getElementById('edit-map-select').value);
if (isNaN(index) || index < 0) return;
currentMapIndex = index;
try {
const response = await fetch(`/api/maps/${index}`);
const map = await response.json();
// Refresh allMaps to get latest isActive status
await loadAllMaps();
// Show active/inactive notice
const isActive = allMaps[index]?.isActive || false;
document.getElementById('active-map-notice').style.display = isActive ? 'block' : 'none';
document.getElementById('inactive-map-notice').style.display = isActive ? 'none' : 'block';
// Populate ignition settings
document.getElementById('dwell-time').value = map.dwellTimeUS;
document.getElementById('rev-limiter').value = map.revLimiterRPM;
document.getElementById('rev-limiter-enabled').checked = map.revLimiterEnabled;
document.getElementById('rev-limiter-cut-pattern').value = map.revLimiterCutPattern !== undefined ? map.revLimiterCutPattern : 2;
// Populate ignition curve table (dynamic points)
currentIgnitionPoints = [];
const ignitionPointCount = map.ignitionPointCount || map.ignitionRPM.length;
for (let i = 0; i < ignitionPointCount; i++) {
currentIgnitionPoints.push({
rpm: map.ignitionRPM[i],
advance: map.ignitionAdvance[i]
});
}
generateIgnitionTable();
// Draw curve
drawIgnitionCurve(map.ignitionRPM.slice(0, ignitionPointCount), map.ignitionAdvance.slice(0, ignitionPointCount));
// Populate quick shifter settings
document.getElementById('qs-enabled').checked = map.qsEnabled;
document.getElementById('qs-sensor-threshold').value = map.qsSensorThreshold;
document.getElementById('qs-sensor-type').value = map.qsSensorInvert ? 'Inverted' : 'Normal';
document.getElementById('qs-min-rpm').value = map.qsMinRPM;
document.getElementById('qs-max-rpm').value = map.qsMaxRPM;
// Populate QS kill time table (dynamic points)
currentQSPoints = [];
const qsPointCount = map.qsPointCount || map.qsRPM.length;
for (let i = 0; i < qsPointCount; i++) {
currentQSPoints.push({
rpm: map.qsRPM[i],
killTime: map.qsKillTime[i]
});
}
generateQSTable();
// Populate settings tab
document.getElementById('map-name').value = map.name;
// Input mode
const acModeRadio = document.querySelector('input[name="input-mode"][value="ac"]');
const dcModeRadio = document.querySelector('input[name="input-mode"][value="dc"]');
if (map.isACMode) {
acModeRadio.checked = true;
} else {
dcModeRadio.checked = true;
}
// AC mode settings
document.getElementById('ac-threshold').value = map.acTriggerThreshold;
document.getElementById('ac-invert').checked = map.acInvertSignal;
// DC mode settings
document.getElementById('dc-pulses').value = map.dcPulsesPerRev;
document.getElementById('dc-pullup').checked = map.dcPullupEnabled;
} catch (error) {
showToast('Failed to load map data', 'error');
}
}
// Generate ignition table (41 points)
function generateIgnitionTable() {
const tbody = document.getElementById('ignition-table-body');
tbody.innerHTML = '';
currentIgnitionPoints.forEach((point, i) => {
const row = tbody.insertRow();
const canDelete = currentIgnitionPoints.length > 2;
row.innerHTML = `
<td>${i + 1}</td>
<td><input type="number" id="rpm-${i}" value="${point.rpm}" min="0" max="25000" step="100" onchange="updateCurveFromTable()"></td>
<td><input type="number" id="adv-${i}" value="${point.advance}" min="-10" max="60" step="1" onchange="updateCurveFromTable()"></td>
<td>
<button class="btn-small btn-danger" onclick="deleteIgnitionPoint(${i})" ${!canDelete ? 'disabled' : ''}>
Hapus
</button>
</td>
`;
});
}
// Generate QS table (dynamic points, RPM-based)
function generateQSTable() {
const tbody = document.getElementById('qs-table-body');
if (!tbody) return;
tbody.innerHTML = '';
currentQSPoints.forEach((point, i) => {
const row = tbody.insertRow();
const canDelete = currentQSPoints.length > 2;
row.innerHTML = `
<td>${i + 1}</td>
<td><input type="number" id="qs-rpm-${i}" value="${point.rpm}" min="0" max="25000" step="100"></td>
<td><input type="number" id="qs-kill-${i}" value="${point.killTime}" min="0" max="300" step="5"></td>
<td>
<button class="btn-small btn-danger" onclick="deleteQSPoint(${i})" ${!canDelete ? 'disabled' : ''}>
Hapus
</button>
</td>
`;
});
}
// Update curve when table changes
function updateCurveFromTable() {
const rpmPoints = [];
const advPoints = [];
currentIgnitionPoints.forEach((_, i) => {
const rpmInput = document.getElementById(`rpm-${i}`);
const advInput = document.getElementById(`adv-${i}`);
if (rpmInput && advInput) {
rpmPoints.push(parseInt(rpmInput.value) || 0);
advPoints.push(parseInt(advInput.value) || 0);
}
});
drawIgnitionCurve(rpmPoints, advPoints);
}
// Draw ignition curve on canvas
function drawIgnitionCurve(rpmPoints, advPoints, currentRPM = null, currentAdvance = null) {
const canvas = document.getElementById('ignition-curve-canvas');
if (!canvas) return;
const ctx = canvas.getContext('2d');
// Clear canvas
ctx.clearRect(0, 0, canvas.width, canvas.height);
// Set up dimensions
const padding = 50;
const width = canvas.width - 2 * padding;
const height = canvas.height - 2 * padding;
// Find min/max
const maxRPM = Math.max(...rpmPoints);
const maxAdv = Math.max(...advPoints);
const minAdv = Math.min(...advPoints);
// Draw axes
ctx.strokeStyle = '#2c3e50';
ctx.lineWidth = 2;
ctx.beginPath();
ctx.moveTo(padding, padding);
ctx.lineTo(padding, canvas.height - padding);
ctx.lineTo(canvas.width - padding, canvas.height - padding);
ctx.stroke();
// Draw grid
ctx.strokeStyle = '#ecf0f1';
ctx.lineWidth = 1;
for (let i = 0; i <= 10; i++) {
const y = padding + (height * i / 10);
ctx.beginPath();
ctx.moveTo(padding, y);
ctx.lineTo(canvas.width - padding, y);
ctx.stroke();
}
// Draw labels
ctx.fillStyle = '#2c3e50';
ctx.font = '12px Arial';
ctx.textAlign = 'right';
for (let i = 0; i <= 5; i++) {
const y = canvas.height - padding - (height * i / 5);
const value = Math.round(minAdv + (maxAdv - minAdv) * i / 5);
ctx.fillText(value + '°', padding - 10, y + 5);
}
ctx.textAlign = 'center';
for (let i = 0; i <= 5; i++) {
const x = padding + (width * i / 5);
const value = Math.round(maxRPM * i / 5);
ctx.fillText(value, x, canvas.height - padding + 20);
}
// Draw axis labels
ctx.save();
ctx.translate(20, canvas.height / 2);
ctx.rotate(-Math.PI / 2);
ctx.textAlign = 'center';
ctx.fillText('Advance (degrees)', 0, 0);
ctx.restore();
ctx.fillText('RPM', canvas.width / 2, canvas.height - 10);
// Draw curve
ctx.strokeStyle = '#667eea';
ctx.lineWidth = 3;
ctx.beginPath();
for (let i = 0; i < rpmPoints.length; i++) {
const x = padding + (width * rpmPoints[i] / maxRPM);
const y = canvas.height - padding - (height * (advPoints[i] - minAdv) / (maxAdv - minAdv));
if (i === 0) {
ctx.moveTo(x, y);
} else {
ctx.lineTo(x, y);
}
}
ctx.stroke();
// Draw points
ctx.fillStyle = '#764ba2';
for (let i = 0; i < rpmPoints.length; i++) {
const x = padding + (width * rpmPoints[i] / maxRPM);
const y = canvas.height - padding - (height * (advPoints[i] - minAdv) / (maxAdv - minAdv));
ctx.beginPath();
ctx.arc(x, y, 4, 0, 2 * Math.PI);
ctx.fill();
}
// Draw live operating point
if (currentRPM !== null && currentAdvance !== null && currentRPM > 0) {
const liveX = padding + (width * currentRPM / maxRPM);
const liveY = canvas.height - padding - (height * (currentAdvance - minAdv) / (maxAdv - minAdv));
// Outer pulsing ring
ctx.strokeStyle = 'rgba(231, 76, 60, 0.5)';
ctx.lineWidth = 3;
ctx.beginPath();
ctx.arc(liveX, liveY, 12, 0, 2 * Math.PI);
ctx.stroke();
// Inner solid circle
ctx.fillStyle = '#e74c3c';
ctx.beginPath();
ctx.arc(liveX, liveY, 6, 0, 2 * Math.PI);
ctx.fill();
// White center dot
ctx.fillStyle = '#ffffff';
ctx.beginPath();
ctx.arc(liveX, liveY, 2, 0, 2 * Math.PI);
ctx.fill();
// Draw crosshair
ctx.strokeStyle = 'rgba(231, 76, 60, 0.8)';
ctx.lineWidth = 1;
ctx.setLineDash([3, 3]);
ctx.beginPath();
ctx.moveTo(padding, liveY);
ctx.lineTo(liveX - 15, liveY);
ctx.moveTo(liveX + 15, liveY);
ctx.lineTo(canvas.width - padding, liveY);
ctx.moveTo(liveX, canvas.height - padding);
ctx.lineTo(liveX, liveY + 15);
ctx.moveTo(liveX, liveY - 15);
ctx.lineTo(liveX, padding);
ctx.stroke();
ctx.setLineDash([]);
// Draw label with RPM and advance
ctx.font = 'bold 12px Arial';
ctx.fillStyle = '#e74c3c';
ctx.textAlign = 'left';
const labelText = `${currentRPM} RPM, ${currentAdvance}°`;
const labelX = liveX + 15;
const labelY = liveY - 15;
// Label background
ctx.fillStyle = 'rgba(255, 255, 255, 0.9)';
const textWidth = ctx.measureText(labelText).width;
ctx.fillRect(labelX - 2, labelY - 12, textWidth + 4, 16);
// Label text
ctx.fillStyle = '#e74c3c';
ctx.fillText(labelText, labelX, labelY);
}
}
// Save ignition configuration
async function saveIgnitionConfig() {
const rpmPoints = [];
const advPoints = [];
// Collect data from table inputs
currentIgnitionPoints.forEach((_, i) => {
const rpmInput = document.getElementById(`rpm-${i}`);
const advInput = document.getElementById(`adv-${i}`);
if (rpmInput && advInput) {
rpmPoints.push(parseInt(rpmInput.value) || 0);
advPoints.push(parseInt(advInput.value) || 0);
}
});
// Validate ascending RPM order
for (let i = 0; i < rpmPoints.length - 1; i++) {
if (rpmPoints[i] >= rpmPoints[i + 1]) {
showToast('Error: RPM values must be in ascending order!', 'warning');
return;
}
}
const parseIntSafe = (id, def) => {
const v = parseInt(document.getElementById(id).value);
return isNaN(v) ? def : v;
};
const mapData = {
name: allMaps[currentMapIndex].name,
engineType: parseIntSafe('engine-type', 0),
pickupSensorOffset: parseIntSafe('pickup-sensor-offset', 0),
dwellTimeUS: parseIntSafe('dwell-time', 3000),
revLimiterRPM: parseIntSafe('rev-limiter', 12000),
revLimiterEnabled: document.getElementById('rev-limiter-enabled').checked,
revLimiterCutPattern: parseIntSafe('rev-limiter-cut-pattern', 2),
ignitionPointCount: rpmPoints.length,
ignitionRPM: rpmPoints,
ignitionAdvance: advPoints
};
try {
const response = await fetch(`/api/maps/${currentMapIndex}`, {
method: 'PUT',
headers: { 'Content-Type': 'application/json' },
body: JSON.stringify(mapData)
});
if (response.ok) {
const isActive = allMaps[currentMapIndex]?.isActive || false;
if (isActive) {
showToast('✓ Saved & Applied! Changes are now active (hot reload). No restart needed.', 'success', 4000);
} else {
showToast('✓ Saved! Map saved to SD card. Switch to this map to apply changes.', 'success', 4000);
}
// Reload map list to update any changes
loadAllMaps();
}
} catch (error) {
showToast('Failed to save configuration', 'error');
}
}
// Save quick shifter configuration (41 points, RPM-based)
async function saveQuickShifterConfig() {
const qsRPM = [];
const qsKillTime = [];
// Collect data from table inputs
currentQSPoints.forEach((_, i) => {
const rpmInput = document.getElementById(`qs-rpm-${i}`);
const killInput = document.getElementById(`qs-kill-${i}`);
if (rpmInput && killInput) {
qsRPM.push(parseInt(rpmInput.value) || 0);
qsKillTime.push(parseInt(killInput.value) || 0);
}
});
// Validate ascending RPM order
for (let i = 0; i < qsRPM.length - 1; i++) {
if (qsRPM[i] >= qsRPM[i + 1]) {
showToast('Error: RPM values must be in ascending order!', 'warning');
return;
}
}
// Helper to parse int with fallback, allowing 0
const parseIntSafe = (id, def) => {
const v = parseInt(document.getElementById(id).value);
return isNaN(v) ? def : v;
};
const mapData = {
name: allMaps[currentMapIndex].name,
qsEnabled: document.getElementById('qs-enabled').checked,
qsSensorThreshold: parseIntSafe('qs-sensor-threshold', 2048),
qsSensorInvert: document.getElementById('qs-sensor-type').value === 'Inverted',
qsMinRPM: parseIntSafe('qs-min-rpm', 3000),
qsMaxRPM: parseIntSafe('qs-max-rpm', 15000),
qsPointCount: qsRPM.length,
qsRPM: qsRPM,
qsKillTime: qsKillTime
};
try {
const response = await fetch(`/api/maps/${currentMapIndex}`, {
method: 'PUT',
headers: { 'Content-Type': 'application/json' },
body: JSON.stringify(mapData)
});
if (response.ok) {
const isActive = allMaps[currentMapIndex]?.isActive || false;
if (isActive) {
showToast('✓ QS Config Saved & Applied! Quick Shifter changes are now active.', 'success', 4000);
} else {
showToast('✓ QS Config Saved! Configuration saved to SD card.', 'success', 4000);
}
loadAllMaps();
}
} catch (error) {
showToast('Failed to save Quick Shifter configuration', 'error');
}
}
// Save general settings
async function saveSettings() {
// Get input mode
const isACMode = document.querySelector('input[name="input-mode"][value="ac"]').checked;
const mapData = {
name: document.getElementById('map-name').value,
isACMode: isACMode,
acTriggerThreshold: parseInt(document.getElementById('ac-threshold').value),
acInvertSignal: document.getElementById('ac-invert').checked,
dcPulsesPerRev: parseInt(document.getElementById('dc-pulses').value),
dcPullupEnabled: document.getElementById('dc-pullup').checked
};
try {
const response = await fetch(`/api/maps/${currentMapIndex}`, {
method: 'PUT',
headers: { 'Content-Type': 'application/json' },
body: JSON.stringify(mapData)
});
if (response.ok) {
const isActive = allMaps[currentMapIndex]?.isActive || false;
if (isActive) {
showToast('✓ Settings saved & applied! Changes are now active.', 'success', 4000);
} else {
showToast('✓ Settings saved! Switch to this map to apply changes.', 'success', 4000);
}
loadAllMaps();
}
} catch (error) {
showToast('Failed to save settings', 'error');
}
}
// Reset to default (8-point template)
async function resetToDefault() {
if (!await showConfirm('Reset ignition curve to default 8-point template?', 'Reset Ignition Curve')) return;
// Default 8-point template
currentIgnitionPoints = [
{ rpm: 0, advance: 5 },
{ rpm: 1000, advance: 10 },
{ rpm: 3000, advance: 20 },
{ rpm: 6000, advance: 30 },
{ rpm: 9000, advance: 35 },
{ rpm: 12000, advance: 32 },
{ rpm: 15000, advance: 28 },
{ rpm: 18000, advance: 25 }
];
generateIgnitionTable();
updateCurveFromTable();
}
// Update live status
async function updateStatus() {
try {
const response = await fetch('/api/status');
const status = await response.json();

// ⚡ Update firmware signature (IMMUTABLE) ⚡
if (status.author) {
  document.getElementById('firmware-author').textContent = '⚡ ' + status.author;
}
if (status.version) {
  document.getElementById('firmware-version').textContent = 'v' + status.version;
}
if (status.copyright) {
  document.getElementById('firmware-copyright').textContent = '© 2025';
}

// Update status bar
document.getElementById('current-rpm').textContent = status.rpm;
document.getElementById('current-speed').textContent = (status.frontWheelSpeed || 0).toFixed(1) + ' km/h';
document.getElementById('current-advance').textContent = status.advance + '°';
// Update ignition curve with live operating point (if on ignition tab and map is active)
if (currentIgnitionPoints.length > 0 && allMaps[currentMapIndex]?.isActive) {
const rpmPoints = currentIgnitionPoints.map(p => p.rpm);
const advPoints = currentIgnitionPoints.map(p => p.advance);
drawIgnitionCurve(rpmPoints, advPoints, status.rpm, status.advance);
}
// QS Status: OFF / READY / ACTIVE
const qsStatusEl = document.getElementById('qs-status');
if (!status.qsEnabled) {
qsStatusEl.textContent = 'OFF';
qsStatusEl.style.color = '#95a5a6';
} else if (status.qsActive) {
qsStatusEl.textContent = 'ACTIVE';
qsStatusEl.style.color = '#f39c12';
} else {
qsStatusEl.textContent = 'READY';
qsStatusEl.style.color = '#2ecc71';
}
// LC Status: OFF / READY / ACTIVE
const lcStatusEl = document.getElementById('lc-status');
if (!status.lcEnabled) {
lcStatusEl.textContent = 'OFF';
lcStatusEl.style.color = '#95a5a6';
} else if (status.lcActive) {
lcStatusEl.textContent = 'ACTIVE';
lcStatusEl.style.color = '#f39c12';
} else {
lcStatusEl.textContent = 'READY';
lcStatusEl.style.color = '#2ecc71';
}
// AW Status: OFF / READY / ACTIVE
const awStatusEl = document.getElementById('aw-status');
if (!status.awEnabled) {
awStatusEl.textContent = 'OFF';
awStatusEl.style.color = '#95a5a6';
} else if (status.awActive) {
awStatusEl.textContent = 'ACTIVE';
awStatusEl.style.color = '#f39c12';
} else {
awStatusEl.textContent = 'READY';
awStatusEl.style.color = '#2ecc71';
}
// TC Status: OFF / READY / ACTIVE
const tcStatusEl = document.getElementById('tc-status');
if (!status.tcEnabled) {
tcStatusEl.textContent = 'OFF';
tcStatusEl.style.color = '#95a5a6';
} else if (status.tcActive) {
tcStatusEl.textContent = 'ACTIVE';
tcStatusEl.style.color = '#f39c12';
} else {
tcStatusEl.textContent = 'READY';
tcStatusEl.style.color = '#2ecc71';
}
// Update calibration status
const calStatusItem = document.getElementById('calibration-status-item');
const calStatus = document.getElementById('calibration-status');
if (status.qsCalibrating) {
calStatusItem.style.display = 'block';
if (status.qsCalibrateStep === 1) {
calStatus.textContent = 'PRESS';
calStatus.style.color = '#f39c12';
} else if (status.qsCalibrateStep === 2) {
calStatus.textContent = 'RELEASE';
calStatus.style.color = '#3498db';
}
} else {
calStatusItem.style.display = 'none';
}
// Update AWTC live stats if on that tab
const awPitchEl = document.getElementById('live-current-pitch');
const frontSpeedEl = document.getElementById('live-front-speed');
const rearSpeedEl = document.getElementById('live-rear-speed');
const slipRatioEl = document.getElementById('live-slip-ratio');
if (awPitchEl) awPitchEl.textContent = status.currentPitch.toFixed(1) + '°';
if (frontSpeedEl) frontSpeedEl.textContent = status.frontWheelSpeed.toFixed(1) + ' km/h';
if (rearSpeedEl) rearSpeedEl.textContent = status.rearWheelSpeed.toFixed(1) + ' km/h';
if (slipRatioEl) slipRatioEl.textContent = (status.slipRatio * 100).toFixed(1) + '%';
// Update statistics
document.getElementById('stat-ignitions').textContent = status.totalIgnitions;
document.getElementById('stat-map').textContent = status.mapName;
// Update RPM color
document.getElementById('current-rpm').style.color = status.rpm > 0 ? '#2ecc71' : '#e74c3c';
// Update Live Monitor Tab
const liveRpmEl = document.getElementById('live-rpm');
if (liveRpmEl) {
liveRpmEl.textContent = status.rpm;
document.getElementById('live-advance').textContent = status.advance + '°';
document.getElementById('live-dwell').textContent = (status.dwellTimeUS || 0) + ' μs';
document.getElementById('live-map-name').textContent = status.mapName;
// Sensor status updates
document.getElementById('live-trigger-sensor').textContent = (status.triggerSensorValue || '-');
document.getElementById('live-trigger-type').textContent = status.isACMode ? 'AC Mode' : 'DC Mode';
document.getElementById('live-qs-sensor').textContent = status.qsSensorValue || 0;
const qsSensorStatus = document.getElementById('live-qs-sensor-status');
if (status.qsEnabled) {
qsSensorStatus.textContent = status.qsActive ? 'AKTIF' : 'READY';
qsSensorStatus.style.color = status.qsActive ? '#f39c12' : '#2ecc71';
} else {
qsSensorStatus.textContent = 'OFF';
qsSensorStatus.style.color = '#95a5a6';
}
// MPU9250 sensor status
const mpuStatus = document.getElementById('live-mpu-status');
const mpuPitch = document.getElementById('live-mpu-pitch');
if (status.awEnabled || status.tcEnabled) {
mpuStatus.textContent = 'AKTIF';
mpuStatus.style.color = '#2ecc71';
mpuPitch.textContent = (status.currentPitch || 0).toFixed(1) + '°';
} else {
mpuStatus.textContent = 'OFF';
mpuStatus.style.color = '#95a5a6';
mpuPitch.textContent = '0.0°';
}
// Clutch sensor
const clutchSensor = document.getElementById('live-clutch-sensor');
if (status.clutchPulled !== undefined) {
clutchSensor.textContent = status.clutchPulled ? 'DITARIK' : 'DILEPAS';
clutchSensor.style.color = status.clutchPulled ? '#f39c12' : '#2ecc71';
} else {
clutchSensor.textContent = '-';
}
const liveQsStatusEl = document.getElementById('live-qs-status');
const liveQsCard = document.getElementById('live-qs-card');
if (!status.qsEnabled) {
liveQsStatusEl.textContent = 'OFF';
liveQsCard.style.background = 'linear-gradient(135deg, #95a5a6 0%, #7f8c8d 100%)';
} else if (status.qsActive) {
liveQsStatusEl.textContent = 'ACTIVE';
liveQsCard.style.background = 'linear-gradient(135deg, #f39c12 0%, #e67e22 100%)';
} else {
liveQsStatusEl.textContent = 'READY';
liveQsCard.style.background = 'linear-gradient(135deg, #2ecc71 0%, #27ae60 100%)';
}
const liveLcStatusEl = document.getElementById('live-lc-status');
const liveLcCard = document.getElementById('live-lc-card');
if (!status.lcEnabled) {
liveLcStatusEl.textContent = 'OFF';
liveLcCard.style.background = 'linear-gradient(135deg, #95a5a6 0%, #7f8c8d 100%)';
} else if (status.lcActive) {
liveLcStatusEl.textContent = 'ACTIVE';
liveLcCard.style.background = 'linear-gradient(135deg, #f39c12 0%, #e67e22 100%)';
} else {
liveLcStatusEl.textContent = 'READY';
liveLcCard.style.background = 'linear-gradient(135deg, #2ecc71 0%, #27ae60 100%)';
}
document.getElementById('live-clutch').textContent = status.clutchPulled ? 'PULLED' : 'RELEASED';
document.getElementById('live-lc-speed').textContent = (status.frontWheelSpeed || 0).toFixed(1) + ' km/h';
const liveAwStatusEl = document.getElementById('live-aw-status');
const liveAwCard = document.getElementById('live-aw-card');
if (!status.awEnabled) {
liveAwStatusEl.textContent = 'OFF';
liveAwCard.style.background = 'linear-gradient(135deg, #95a5a6 0%, #7f8c8d 100%)';
} else if (status.awActive) {
liveAwStatusEl.textContent = 'ACTIVE';
liveAwCard.style.background = 'linear-gradient(135deg, #f39c12 0%, #e67e22 100%)';
} else {
liveAwStatusEl.textContent = 'READY';
liveAwCard.style.background = 'linear-gradient(135deg, #2ecc71 0%, #27ae60 100%)';
}
document.getElementById('live-pitch').textContent = (status.currentPitch || 0).toFixed(1) + '°';
const liveTcStatusEl = document.getElementById('live-tc-status');
const liveTcCard = document.getElementById('live-tc-card');
if (!status.tcEnabled) {
liveTcStatusEl.textContent = 'OFF';
liveTcCard.style.background = 'linear-gradient(135deg, #95a5a6 0%, #7f8c8d 100%)';
} else if (status.tcActive) {
liveTcStatusEl.textContent = 'ACTIVE';
liveTcCard.style.background = 'linear-gradient(135deg, #f39c12 0%, #e67e22 100%)';
} else {
liveTcStatusEl.textContent = 'READY';
liveTcCard.style.background = 'linear-gradient(135deg, #2ecc71 0%, #27ae60 100%)';
}
document.getElementById('live-slip').textContent = ((status.slipRatio || 0) * 100).toFixed(1) + '%';
document.getElementById('live-front-wheel').textContent = (status.frontWheelSpeed || 0).toFixed(1) + ' km/h';
document.getElementById('live-rear-wheel').textContent = (status.rearWheelSpeed || 0).toFixed(1) + ' km/h';
const liveRevlimitStatusEl = document.getElementById('live-revlimit-status');
const liveRevlimitCard = document.getElementById('live-revlimit-card');
if (status.revLimiterActive) {
liveRevlimitStatusEl.textContent = 'ACTIVE';
liveRevlimitCard.style.background = 'linear-gradient(135deg, #e74c3c 0%, #c0392b 100%)';
} else {
liveRevlimitStatusEl.textContent = 'INACTIVE';
liveRevlimitCard.style.background = 'linear-gradient(135deg, #2ecc71 0%, #27ae60 100%)';
}
document.getElementById('live-revlimit-rpm').textContent = status.revLimiterRPM || 0;
document.getElementById('live-total-ignitions').textContent = status.totalIgnitions || 0;

// ===== EDGE PROTECTION: Emergency Status Monitoring =====
checkEmergencyStatus();
}
} catch (error) {
}
}

// ===== Emergency Status Monitoring =====
let lastEmergencyCheck = 0;
let emergencyAlertShown = false;

async function checkEmergencyStatus() {
  try {
    const response = await fetch('/api/emergency?action=status', { method: 'POST' });
    const emergency = await response.json();

    // Update emergency indicator in UI
    const emergencyIndicator = document.getElementById('emergency-indicator') || createEmergencyIndicator();

    if (emergency.emergencyShutdown) {
      // CRITICAL: Emergency shutdown active
      emergencyIndicator.className = 'emergency-indicator emergency-critical';
      emergencyIndicator.innerHTML = '🔴 EMERGENCY SHUTDOWN';
      emergencyIndicator.style.display = 'block';

      // Show alert once
      if (!emergencyAlertShown) {
        const reasons = [];
        if (emergency.shutdownReason & 0x01) reasons.push('Coil Overheat');
        if (emergency.shutdownReason & 0x02) reasons.push('Low Voltage');
        if (emergency.shutdownReason & 0x04) reasons.push('Sensor Failure');
        if (emergency.shutdownReason & 0x08) reasons.push('Persistent Misfires');
        if (emergency.shutdownReason & 0x10) reasons.push('User Initiated');

        showToast(`🔴 EMERGENCY: ${reasons.join(', ')}`, 'error', 10000);
        emergencyAlertShown = true;
      }
    } else if (emergency.emergencySafeMode) {
      // WARNING: Safe mode active
      emergencyIndicator.className = 'emergency-indicator emergency-warning';
      emergencyIndicator.innerHTML = '⚠️ SAFE MODE';
      emergencyIndicator.style.display = 'block';
    } else if (emergency.coilProtection) {
      // WARNING: Coil protection active
      emergencyIndicator.className = 'emergency-indicator emergency-warning';
      emergencyIndicator.innerHTML = '⚠️ COIL PROTECTION';
      emergencyIndicator.style.display = 'block';
    } else if (emergency.consecutiveMisfires > 50) {
      // WARNING: High misfires
      emergencyIndicator.className = 'emergency-indicator emergency-warning';
      emergencyIndicator.innerHTML = `⚠️ MISFIRES: ${emergency.consecutiveMisfires}`;
      emergencyIndicator.style.display = 'block';
    } else if (emergency.rpmGlitches > 10) {
      // WARNING: Sensor glitches
      emergencyIndicator.className = 'emergency-indicator emergency-warning';
      emergencyIndicator.innerHTML = `⚠️ SENSOR GLITCHES: ${emergency.rpmGlitches}`;
      emergencyIndicator.style.display = 'block';
    } else {
      // All OK
      emergencyIndicator.style.display = 'none';
      emergencyAlertShown = false;
    }

    // Update voltage display with warning
    const voltageEl = document.getElementById('battery-voltage') || createVoltageDisplay();
    voltageEl.textContent = `${emergency.batteryVoltage.toFixed(1)}V`;
    if (emergency.batteryVoltage < 11.0) {
      voltageEl.style.color = '#e74c3c';  // Red
      voltageEl.textContent += ' ⚠️';
    } else if (emergency.batteryVoltage < 11.5) {
      voltageEl.style.color = '#f39c12';  // Orange
    } else {
      voltageEl.style.color = '#2ecc71';  // Green
    }

  } catch (error) {
    console.error('Failed to check emergency status:', error);
  }
}

function createEmergencyIndicator() {
  const indicator = document.createElement('div');
  indicator.id = 'emergency-indicator';
  indicator.className = 'emergency-indicator';
  indicator.style.display = 'none';
  indicator.onclick = () => showEmergencyDetails();

  // Insert at top of body
  document.body.insertBefore(indicator, document.body.firstChild);
  return indicator;
}

function createVoltageDisplay() {
  const voltageEl = document.createElement('div');
  voltageEl.id = 'battery-voltage';
  voltageEl.className = 'battery-voltage';

  // Add to status bar
  const statusBar = document.querySelector('.status-bar') || document.body;
  statusBar.appendChild(voltageEl);
  return voltageEl;
}

async function showEmergencyDetails() {
  try {
    const response = await fetch('/api/emergency?action=status', { method: 'POST' });
    const emergency = await response.json();

    const reasons = [];
    if (emergency.shutdownReason & 0x01) reasons.push('🔥 Coil Overheat');
    if (emergency.shutdownReason & 0x02) reasons.push('🔋 Low Voltage');
    if (emergency.shutdownReason & 0x04) reasons.push('📡 Sensor Failure');
    if (emergency.shutdownReason & 0x08) reasons.push('💥 Persistent Misfires');
    if (emergency.shutdownReason & 0x10) reasons.push('👤 User Initiated');

    const details = `
      <strong>Emergency Status:</strong><br>
      Shutdown: ${emergency.emergencyShutdown ? 'YES' : 'NO'}<br>
      Safe Mode: ${emergency.emergencySafeMode ? 'YES' : 'NO'}<br>
      Coil Protection: ${emergency.coilProtection ? 'YES' : 'NO'}<br>
      <br>
      <strong>Diagnostics:</strong><br>
      Consecutive Misfires: ${emergency.consecutiveMisfires}<br>
      RPM Glitches: ${emergency.rpmGlitches}<br>
      Low Voltage Counter: ${emergency.lowVoltageCounter}<br>
      Battery: ${emergency.batteryVoltage.toFixed(2)}V (min: ${emergency.minVoltage.toFixed(2)}V)<br>
      <br>
      ${reasons.length > 0 ? '<strong>Shutdown Reasons:</strong><br>' + reasons.join('<br>') : ''}
    `;

    const confirmed = await showConfirm(details, '🛡️ Emergency Status');
    if (confirmed && emergency.emergencyShutdown) {
      const resetConfirm = await showConfirm(
        'Reset emergency state? This will re-enable ignition. Only do this if you fixed the issue!',
        '⚠️ Reset Emergency'
      );
      if (resetConfirm) {
        await fetch('/api/emergency?action=reset', { method: 'POST' });
        showToast('✅ Emergency state reset', 'success');
        emergencyAlertShown = false;
      }
    }
  } catch (error) {
    showToast('Failed to get emergency details', 'error');
  }
}
// QS Calibration Functions
async function startQSCalibration() {
try {
const response = await fetch('/api/calibrateQS/start', { method: 'POST' });
const data = await response.json();
if (data.success) {
document.getElementById('calibration-steps').style.display = 'block';
document.getElementById('cal-step-1').style.display = 'block';
document.getElementById('cal-step-2').style.display = 'none';
document.getElementById('cal-complete').style.display = 'none';
// Start polling sensor values
calibrationInterval = setInterval(updateCalibrationSensor, 200);
}
} catch (error) {
showToast('Failed to start calibration', 'error');
}
}
async function updateCalibrationSensor() {
try {
const response = await fetch('/api/calibrateQS/status');
const data = await response.json();
if (data.step === 1) {
document.getElementById('cal-sensor-value-1').textContent = data.sensorValue;
} else if (data.step === 2) {
document.getElementById('cal-sensor-value-2').textContent = data.sensorValue;
}
if (!data.calibrating) {
clearInterval(calibrationInterval);
}
} catch (error) {
}
}
async function capturePressed() {
try {
const response = await fetch('/api/calibrateQS/capture', { method: 'POST' });
const data = await response.json();
if (data.success && data.step === 2) {
document.getElementById('cal-step-1').style.display = 'none';
document.getElementById('cal-step-2').style.display = 'block';
}
} catch (error) {
showToast('Failed to capture value', 'error');
}
}
async function captureReleased() {
try {
const response = await fetch('/api/calibrateQS/capture', { method: 'POST' });
const data = await response.json();
if (data.success) {
clearInterval(calibrationInterval);
// Show results
document.getElementById('cal-step-2').style.display = 'none';
document.getElementById('cal-complete').style.display = 'block';
document.getElementById('cal-result-pressed').textContent = data.pressedValue;
document.getElementById('cal-result-released').textContent = data.releasedValue;
document.getElementById('cal-result-threshold').textContent = data.threshold;
document.getElementById('cal-result-inverted').textContent = data.inverted ? 'Inverted' : 'Normal';
// Update read-only fields
document.getElementById('qs-sensor-threshold').value = data.threshold;
document.getElementById('qs-sensor-type').value = data.inverted ? 'Inverted' : 'Normal';
// Reload map data
setTimeout(() => {
loadMapForEdit();
}, 1000);
}
} catch (error) {
showToast('Failed to complete calibration', 'error');
}
}
async function cancelCalibration() {
try {
await fetch('/api/calibrateQS/cancel', { method: 'POST' });
clearInterval(calibrationInterval);
closeCalibration();
} catch (error) {
}
}
function closeCalibration() {
document.getElementById('calibration-steps').style.display = 'none';
document.getElementById('cal-step-1').style.display = 'none';
document.getElementById('cal-step-2').style.display = 'none';
document.getElementById('cal-complete').style.display = 'none';
clearInterval(calibrationInterval);
}
// ========================================
// Launch Control Functions
// ========================================
// Populate LC map select dropdown
function populateLCMapSelect() {
const select = document.getElementById('lc-edit-map-select');
select.innerHTML = '';
allMaps.forEach((map, index) => {
const option = document.createElement('option');
option.value = index;
option.textContent = `${map.name}${map.isActive ? ' (Active)' : ''}`;
select.appendChild(option);
});
}
// Load map for LC editing
async function loadMapForLCEdit() {
const index = parseInt(document.getElementById('lc-edit-map-select').value);
if (isNaN(index) || index < 0) {
return;
}
currentMapIndex = index;
try {
const response = await fetch(`/api/maps/${index}`);
const map = await response.json();
// Refresh allMaps to get latest isActive status
await loadAllMaps();
// Populate LC form
document.getElementById('lc-enabled').checked = map.lcEnabled || false;
document.getElementById('lc-target-rpm').value = map.lcTargetRPM || 6000;
document.getElementById('lc-retard-degrees').value = map.lcRetardDegrees !== undefined ? map.lcRetardDegrees : 10;
document.getElementById('lc-cut-pattern').value = map.lcCutPattern !== undefined ? map.lcCutPattern : 2;
// Determine if this map is active
const isActive = allMaps[index]?.isActive || false;
// Show active/inactive notice
document.getElementById('lc-active-map-notice').style.display = isActive ? 'block' : 'none';
document.getElementById('lc-inactive-map-notice').style.display = isActive ? 'none' : 'block';
} catch (error) {
showToast('Failed to load map data', 'error');
}
}
// Save Launch Control configuration
async function saveLaunchControlConfig() {
// Helper to parse int with fallback, allowing 0
const parseIntSafe = (id, def) => {
const v = parseInt(document.getElementById(id).value);
return isNaN(v) ? def : v;
};
const lcData = {
lcEnabled: document.getElementById('lc-enabled').checked,
lcTargetRPM: parseIntSafe('lc-target-rpm', 6000),
lcRetardDegrees: parseIntSafe('lc-retard-degrees', 10),
lcCutPattern: parseIntSafe('lc-cut-pattern', 2)
};
try {
const response = await fetch(`/api/maps/${currentMapIndex}`, {
method: 'PUT',
headers: { 'Content-Type': 'application/json' },
body: JSON.stringify(lcData)
});
if (response.ok) {
// Refresh maps to get updated isActive status
await loadAllMaps();
// Determine if this is active map
const isActive = allMaps[currentMapIndex]?.isActive || false;
// Reload LC form to update notices
await loadMapForLCEdit();
if (isActive) {
showToast('✓ LC Config Saved & Applied! Changes are now active.', 'success', 4000);
} else {
showToast('✓ LC Config Saved! Switch to this map to apply changes.', 'success', 4000);
}
}
} catch (error) {
showToast('Failed to save LC configuration', 'error');
}
}
// Calculate and display wheel circumferences based on tire configuration
function calculateWheelCircumferences() {
// Get front tire configuration
const frontWidth = parseInt(document.getElementById('tc-front-tire-width').value) || 70;
const frontAspect = parseInt(document.getElementById('tc-front-tire-aspect').value) || 80;
const frontDiameter = parseInt(document.getElementById('tc-front-wheel-diameter').value) || 17;
// Get rear tire configuration
const rearWidth = parseInt(document.getElementById('tc-rear-tire-width').value) || 80;
const rearAspect = parseInt(document.getElementById('tc-rear-tire-aspect').value) || 90;
const rearDiameter = parseInt(document.getElementById('tc-rear-wheel-diameter').value) || 17;
// Calculate front wheel circumference
// Formula: Circumference = π × (Wheel Diameter + 2 × Sidewall Height)
// Where: Sidewall Height = Tire Width × Aspect Ratio / 100
const frontWheelDiameterMM = frontDiameter * 25.4;
const frontSidewallHeight = (frontWidth * frontAspect) / 100.0;
const frontTotalDiameterMM = frontWheelDiameterMM + (2.0 * frontSidewallHeight);
const frontCircumferenceM = (Math.PI * frontTotalDiameterMM) / 1000.0;
// Calculate rear wheel circumference
const rearWheelDiameterMM = rearDiameter * 25.4;
const rearSidewallHeight = (rearWidth * rearAspect) / 100.0;
const rearTotalDiameterMM = rearWheelDiameterMM + (2.0 * rearSidewallHeight);
const rearCircumferenceM = (Math.PI * rearTotalDiameterMM) / 1000.0;
// Update display
const frontCircDisplay = document.getElementById('front-circumference');
const rearCircDisplay = document.getElementById('rear-circumference');
if (frontCircDisplay) {
frontCircDisplay.textContent = `${frontCircumferenceM.toFixed(3)} m (${frontTotalDiameterMM.toFixed(1)} mm diameter)`;
}
if (rearCircDisplay) {
rearCircDisplay.textContent = `${rearCircumferenceM.toFixed(3)} m (${rearTotalDiameterMM.toFixed(1)} mm diameter)`;
}
}
// Load map for AWTC editing
async function loadMapForAWTCEdit() {
const index = parseInt(document.getElementById('awtc-edit-map-select').value);
if (isNaN(index) || index < 0) {
return;
}
currentMapIndex = index;
try {
const response = await fetch(`/api/maps/${index}`);
const map = await response.json();
// Refresh allMaps to get latest isActive status
await loadAllMaps();
// Populate Anti-Wheelie form
document.getElementById('aw-enabled').checked = map.awEnabled || false;
document.getElementById('aw-pitch-threshold').value = map.awPitchThreshold !== undefined ? map.awPitchThreshold : 15.0;
document.getElementById('aw-cut-pattern').value = map.awCutPattern !== undefined ? map.awCutPattern : 2;
document.getElementById('aw-retard-degrees').value = map.awRetardDegrees !== undefined ? map.awRetardDegrees : 5;
// Populate Traction Control form
document.getElementById('tc-enabled').checked = map.tcEnabled || false;
document.getElementById('tc-front-wheel-holes').value = map.tcFrontWheelHoles || 4;
document.getElementById('tc-rear-wheel-holes').value = map.tcRearWheelHoles || 4;
document.getElementById('tc-slip-threshold').value = map.tcSlipThreshold !== undefined ? map.tcSlipThreshold : 0.15;
document.getElementById('tc-cut-pattern').value = map.tcCutPattern !== undefined ? map.tcCutPattern : 2;
document.getElementById('tc-retard-degrees').value = map.tcRetardDegrees !== undefined ? map.tcRetardDegrees : 5;
// Populate Tire Configuration form
document.getElementById('tc-front-tire-width').value = map.tcFrontTireWidth || 70;
document.getElementById('tc-front-tire-aspect').value = map.tcFrontTireAspect || 80;
document.getElementById('tc-front-wheel-diameter').value = map.tcFrontWheelDiameter || 17;
document.getElementById('tc-rear-tire-width').value = map.tcRearTireWidth || 80;
document.getElementById('tc-rear-tire-aspect').value = map.tcRearTireAspect || 90;
document.getElementById('tc-rear-wheel-diameter').value = map.tcRearWheelDiameter || 17;
// Calculate and display circumferences
calculateWheelCircumferences();
// Determine if this map is active
const isActive = allMaps[index]?.isActive || false;
// Show active/inactive notice
document.getElementById('awtc-active-map-notice').style.display = isActive ? 'block' : 'none';
document.getElementById('awtc-inactive-map-notice').style.display = isActive ? 'none' : 'block';
} catch (error) {
showToast('Failed to load map data', 'error');
}
}
// Save AWTC configuration
async function saveAWTCConfig() {
// Helper to parse with fallback, allowing 0
const parseIntSafe = (id, def) => {
const v = parseInt(document.getElementById(id).value);
return isNaN(v) ? def : v;
};
const parseFloatSafe = (id, def) => {
const v = parseFloat(document.getElementById(id).value);
return isNaN(v) ? def : v;
};
const awtcData = {
awEnabled: document.getElementById('aw-enabled').checked,
awPitchThreshold: parseFloatSafe('aw-pitch-threshold', 15.0),
awCutPattern: parseIntSafe('aw-cut-pattern', 2),
awRetardDegrees: parseIntSafe('aw-retard-degrees', 5),
tcEnabled: document.getElementById('tc-enabled').checked,
tcFrontWheelHoles: parseIntSafe('tc-front-wheel-holes', 4),
tcRearWheelHoles: parseIntSafe('tc-rear-wheel-holes', 4),
tcSlipThreshold: parseFloatSafe('tc-slip-threshold', 0.15),
tcCutPattern: parseIntSafe('tc-cut-pattern', 2),
tcRetardDegrees: parseIntSafe('tc-retard-degrees', 5),
// Tire configuration for accurate speed calculation
tcFrontTireWidth: parseIntSafe('tc-front-tire-width', 70),
tcFrontTireAspect: parseIntSafe('tc-front-tire-aspect', 80),
tcFrontWheelDiameter: parseIntSafe('tc-front-wheel-diameter', 17),
tcRearTireWidth: parseIntSafe('tc-rear-tire-width', 80),
tcRearTireAspect: parseIntSafe('tc-rear-tire-aspect', 90),
tcRearWheelDiameter: parseIntSafe('tc-rear-wheel-diameter', 17)
};
try {
const response = await fetch(`/api/maps/${currentMapIndex}`, {
method: 'PUT',
headers: { 'Content-Type': 'application/json' },
body: JSON.stringify(awtcData)
});
if (response.ok) {
// Refresh maps to get updated isActive status
await loadAllMaps();
// Determine if this is active map
const isActive = allMaps[currentMapIndex]?.isActive || false;
// Reload AWTC form to update notices
await loadMapForAWTCEdit();
if (isActive) {
showToast('✓ AW & TC Config Saved & Applied! Changes are now active.', 'success', 4000);
} else {
showToast('✓ AW & TC Config Saved! Switch to this map to apply changes.', 'success', 4000);
}
}
} catch (error) {
showToast('Failed to save AW & TC configuration', 'error');
}
}
// === Dynamic Point Management Functions ===
// Add new ignition point
function addIgnitionPoint() {
if (currentIgnitionPoints.length >= 50) {
showToast('Maximum 50 points allowed!', 'warning');
return;
}
// Prompt for RPM value
const rpm = prompt('Enter RPM value (0-25000):');
if (rpm === null) return;
const rpmValue = parseInt(rpm);
if (isNaN(rpmValue) || rpmValue < 0 || rpmValue > 25000) {
showToast('Invalid RPM value!', 'warning');
return;
}
// Check for duplicate
if (currentIgnitionPoints.some(p => p.rpm === rpmValue)) {
showToast('RPM value already exists!', 'warning');
return;
}
// Prompt for advance value
const advance = prompt('Enter advance degrees (-10 to 60):');
if (advance === null) return;
const advanceValue = parseInt(advance);
if (isNaN(advanceValue) || advanceValue < -10 || advanceValue > 60) {
showToast('Invalid advance value!', 'warning');
return;
}
// Add point
currentIgnitionPoints.push({ rpm: rpmValue, advance: advanceValue });
// Sort by RPM
currentIgnitionPoints.sort((a, b) => a.rpm - b.rpm);
// Regenerate table
generateIgnitionTable();
updateCurveFromTable();
}
// Delete ignition point
async function deleteIgnitionPoint(index) {
if (currentIgnitionPoints.length <= 2) {
showToast('Cannot delete - minimum 2 points required!', 'warning');
return;
}
if (!await showConfirm(`Delete point at ${currentIgnitionPoints[index].rpm} RPM?`, 'Delete Point')) {
return;
}
currentIgnitionPoints.splice(index, 1);
generateIgnitionTable();
updateCurveFromTable();
}
// Add new QS point
function addQSPoint() {
if (currentQSPoints.length >= 50) {
showToast('Maximum 50 points allowed!', 'warning');
return;
}
// Prompt for RPM value
const rpm = prompt('Enter RPM value (0-25000):');
if (rpm === null) return;
const rpmValue = parseInt(rpm);
if (isNaN(rpmValue) || rpmValue < 0 || rpmValue > 25000) {
showToast('Invalid RPM value!', 'warning');
return;
}
// Check for duplicate
if (currentQSPoints.some(p => p.rpm === rpmValue)) {
showToast('RPM value already exists!', 'warning');
return;
}
// Prompt for kill time value
const killTime = prompt('Enter kill time in milliseconds (0-300):');
if (killTime === null) return;
const killTimeValue = parseInt(killTime);
if (isNaN(killTimeValue) || killTimeValue < 0 || killTimeValue > 300) {
showToast('Invalid kill time value!', 'warning');
return;
}
// Add point
currentQSPoints.push({ rpm: rpmValue, killTime: killTimeValue });
// Sort by RPM
currentQSPoints.sort((a, b) => a.rpm - b.rpm);
// Regenerate table
generateQSTable();
}
// Delete QS point
async function deleteQSPoint(index) {
if (currentQSPoints.length <= 2) {
showToast('Cannot delete - minimum 2 points required!', 'warning');
return;
}
if (!await showConfirm(`Delete point at ${currentQSPoints[index].rpm} RPM?`, 'Delete Point')) {
return;
}
currentQSPoints.splice(index, 1);
generateQSTable();
}
// === Map CRUD Functions ===
// Create new map
async function createNewMap() {
const name = prompt('Enter map name:');
if (!name) return;
try {
const response = await fetch('/api/maps/create', {
method: 'POST',
headers: { 'Content-Type': 'application/json' },
body: JSON.stringify({ name: name })
});
const result = await response.json();
if (result.success) {
showToast(`Map created successfully! ID: ${result.mapId}`, 'success');
await loadAllMaps();
} else {
showToast('Failed to create map: ' + (result.error || 'Unknown error'));
}
} catch (error) {
showToast('Failed to create map', 'error');
}
}
// Delete map
async function deleteMap(mapId) {
const map = allMaps.find(m => m.id === mapId);
if (!map) return;
if (!await showConfirm(`Delete map "${map.name}"? This cannot be undone!`, 'Delete Map')) {
return;
}
try {
const response = await fetch('/api/maps/delete', {
method: 'POST',
headers: { 'Content-Type': 'application/json' },
body: JSON.stringify({ mapId: mapId })
});
const result = await response.json();
if (result.success) {
showToast('Map deleted successfully!', 'success');
await loadAllMaps();
} else {
showToast('Failed to delete map: ' + (result.error || 'Unknown error'));
}
} catch (error) {
showToast('Failed to delete map', 'error');
}
}
// Duplicate map
async function duplicateMap(mapId) {
const map = allMaps.find(m => m.id === mapId);
if (!map) return;
if (!await showConfirm(`Duplicate map "${map.name}"?`, 'Duplicate Map')) {
return;
}
try {
const response = await fetch('/api/maps/duplicate', {
method: 'POST',
headers: { 'Content-Type': 'application/json' },
body: JSON.stringify({ sourceMapId: mapId })
});
const result = await response.json();
if (result.success) {
showToast(`Map duplicated successfully! New ID: ${result.newMapId}`, 'success');
await loadAllMaps();
} else {
showToast('Failed to duplicate map: ' + (result.error || 'Unknown error'));
}
} catch (error) {
showToast('Failed to duplicate map', 'error');
}
}
// Add event listeners for tire configuration inputs to auto-update circumferences
document.addEventListener('DOMContentLoaded', function() {
// Array of all tire input field IDs
const tireInputIds = [
'tc-front-tire-width',
'tc-front-tire-aspect',
'tc-front-wheel-diameter',
'tc-rear-tire-width',
'tc-rear-tire-aspect',
'tc-rear-wheel-diameter'
];
// Add input event listener to each field
tireInputIds.forEach(id => {
const element = document.getElementById(id);
if (element) {
element.addEventListener('input', calculateWheelCircumferences);
}
});
});
// Restart device function
async function restartDevice() {
if (!await showConfirm('Restart device now? You will lose connection temporarily.', 'Restart Device')) return;
try {
await fetch('/api/restart', { method: 'POST' });
showToast('Device is restarting... You may need to reconnect.', 'info', 5000);
} catch (error) {
// Connection lost is expected during restart
showToast('Device is restarting... You may need to reconnect.', 'info', 5000);
}
}
// Close modal on outside click
window.onclick = function(event) {
const modal = document.getElementById('modal');
if (event.target == modal) {
closeModal();
}
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 📋 LOG VIEWER FUNCTIONS
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

let allLogs = [];
let autoRefreshLogsEnabled = false;
let logsRefreshInterval = null;

// Fetch and display logs
async function refreshLogs() {
  try {
    const count = document.getElementById('log-count').value;
    const response = await fetch(`/api/logs?count=${count}`);
    const data = await response.json();

    allLogs = data.logs || [];

    // Update stats
    const stats = `Total: ${data.totalLogs || 0} | Ring Buffer: ${data.bufferLogs || 0}/${data.bufferSize || 0} | Dropped: ${data.droppedLogs || 0}`;
    document.getElementById('log-stats').textContent = stats;

    // Display logs
    filterLogs();

  } catch (error) {
    console.error('Failed to fetch logs:', error);
    document.getElementById('log-viewer').innerHTML =
      '<div class="log-entry log-error">❌ Failed to load logs</div>';
  }
}

// Filter logs by level
function filterLogs() {
  const filterLevel = document.getElementById('log-filter-level').value;
  const logViewer = document.getElementById('log-viewer');

  if (allLogs.length === 0) {
    logViewer.innerHTML = '<div class="log-entry log-loading">No logs available</div>';
    return;
  }

  // Filter logs
  let filteredLogs = allLogs;
  if (filterLevel !== 'ALL') {
    filteredLogs = allLogs.filter(log => log.level === filterLevel);
  }

  if (filteredLogs.length === 0) {
    logViewer.innerHTML = `<div class="log-entry log-loading">No ${filterLevel} logs found</div>`;
    return;
  }

  // Display logs (newest first)
  let html = '';
  for (let i = filteredLogs.length - 1; i >= 0; i--) {
    const log = filteredLogs[i];
    const levelClass = log.level.toLowerCase();
    const levelBadge = log.level === 'INFO' ? 'ℹ️' :
                      log.level === 'WARNING' ? '⚠️' : '❌';

    html += `
      <div class="log-entry log-${levelClass}">
        <span class="log-timestamp">[${log.timestamp || 'N/A'}]</span>
        <span class="log-level level-${levelClass}">${levelBadge} ${log.level}</span>
        <span class="log-message">${log.message || ''}</span>
      </div>
    `;
  }

  logViewer.innerHTML = html;

  // Auto-scroll to bottom (newest logs)
  logViewer.scrollTop = 0;
}

// Toggle auto-refresh for logs
function toggleAutoRefreshLogs() {
  autoRefreshLogsEnabled = !autoRefreshLogsEnabled;
  const btn = document.getElementById('auto-refresh-logs-btn');

  if (autoRefreshLogsEnabled) {
    btn.textContent = '🔄 Auto-Refresh: ON';
    btn.parentElement.classList.add('btn-primary');
    btn.parentElement.classList.remove('btn-secondary');

    // Refresh immediately
    refreshLogs();

    // Start interval (every 3 seconds)
    logsRefreshInterval = setInterval(refreshLogs, 3000);
  } else {
    btn.textContent = '🔄 Auto-Refresh: OFF';
    btn.parentElement.classList.remove('btn-primary');
    btn.parentElement.classList.add('btn-secondary');

    // Stop interval
    if (logsRefreshInterval) {
      clearInterval(logsRefreshInterval);
      logsRefreshInterval = null;
    }
  }
}

// Download logs as file
async function downloadLogs() {
  try {
    const response = await fetch('/api/logs/download');
    const blob = await response.blob();

    // Create download link
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `cdi_logs_${Date.now()}.txt`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    window.URL.revokeObjectURL(url);

    showToast('✅ Logs downloaded successfully', 'success');
  } catch (error) {
    console.error('Failed to download logs:', error);
    showToast('❌ Failed to download logs', 'error');
  }
}

// Load logs when logs tab is opened
document.addEventListener('DOMContentLoaded', function() {
  // Add event listener for tab changes
  const tabButtons = document.querySelectorAll('.tab-btn');
  tabButtons.forEach(btn => {
    btn.addEventListener('click', function() {
      const tab = this.getAttribute('data-tab');
      if (tab === 'logs') {
        // Load logs when logs tab is opened
        refreshLogs();
      } else if (tab === 'settings') {
        // Load file list and simulator status when settings tab is opened
        refreshFileList();
        refreshSimulatorStatus();
      }
    });
  });
});

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 📁 FILE MANAGER FUNCTIONS
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

let allFiles = [];

// Fetch and display file list
async function refreshFileList() {
  try {
    const response = await fetch('/api/files');
    const data = await response.json();

    allFiles = data.files || [];

    // Update stats
    const totalFiles = allFiles.length;
    const totalSize = allFiles.reduce((sum, file) => sum + (file.size || 0), 0);
    const stats = `Total Files: ${totalFiles} | Total Size: ${formatBytes(totalSize)}`;
    document.getElementById('file-stats').textContent = stats;

    // Display files
    filterFiles();

  } catch (error) {
    console.error('Failed to fetch files:', error);
    document.getElementById('file-list').innerHTML =
      '<div class="file-entry file-loading">❌ Failed to load files</div>';
  }
}

// Filter files by type
function filterFiles() {
  const filterType = document.getElementById('file-filter-type').value;
  const fileList = document.getElementById('file-list');

  if (allFiles.length === 0) {
    fileList.innerHTML = '<div class="file-entry file-loading">No files found</div>';
    return;
  }

  // Filter files
  let filteredFiles = allFiles;
  if (filterType !== 'ALL') {
    filteredFiles = allFiles.filter(file => file.type === filterType);
  }

  if (filteredFiles.length === 0) {
    fileList.innerHTML = `<div class="file-entry file-loading">No ${filterType} files found</div>`;
    return;
  }

  // Display files
  let html = '';
  filteredFiles.forEach(file => {
    const typeClass = `file-${file.type}`;
    const typeIcon = file.type === 'map' ? '🗺️' :
                    file.type === 'config' ? '⚙️' :
                    file.type === 'web' ? '🌐' : '📋';

    html += `
      <div class="file-entry ${typeClass}">
        <div class="file-info">
          <div class="file-name">${typeIcon} ${file.name}</div>
          <div class="file-meta">
            <span class="file-type-badge type-${file.type}">${file.type.toUpperCase()}</span>
            ${formatBytes(file.size)} • ${file.path}
          </div>
        </div>
        <div class="file-actions">
          <button class="btn-download" onclick="downloadFile('${file.path}', '${file.name}')">
            💾 Download
          </button>
        </div>
      </div>
    `;
  });

  fileList.innerHTML = html;
}

// Download file
function downloadFile(path, filename) {
  const link = document.createElement('a');
  link.href = '/download?path=' + encodeURIComponent(path);
  link.download = filename;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  showToast('💾 Downloading: ' + filename, 'success');
}

// Format bytes to human readable
function formatBytes(bytes) {
  if (bytes === 0) return '0 Bytes';
  const k = 1024;
  const sizes = ['Bytes', 'KB', 'MB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 🎮 SIMULATOR CONTROL FUNCTIONS
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

let simulatorEnabled = false;
let simulatorRunning = false;

// Refresh simulator status
async function refreshSimulatorStatus() {
  try {
    const response = await fetch('/api/simulator');
    const data = await response.json();

    simulatorEnabled = data.enabled || false;
    simulatorRunning = data.running || false;

    // Update UI
    document.getElementById('sim-enabled-status').textContent = simulatorEnabled ? '✅ Enabled' : '❌ Disabled';
    document.getElementById('sim-running-status').textContent = simulatorRunning ? '▶️ Running' : '⏹️ Stopped';
    document.getElementById('sim-virtual-rpm').textContent = data.virtualRPM || 0;

    // Update button states
    const toggleBtn = document.getElementById('sim-toggle-btn');
    const startBtn = document.getElementById('sim-start-btn');
    const stopBtn = document.getElementById('sim-stop-btn');
    const rpmSlider = document.getElementById('sim-rpm-slider');
    const speedInput = document.getElementById('sim-speed');
    const clutchInput = document.getElementById('sim-clutch');
    const applyBtn = document.getElementById('sim-apply-btn');

    if (simulatorEnabled) {
      toggleBtn.textContent = '🎮 Disable Simulator';
      toggleBtn.classList.remove('btn-secondary');
      toggleBtn.classList.add('btn-danger');

      startBtn.disabled = simulatorRunning;
      stopBtn.disabled = !simulatorRunning;
      rpmSlider.disabled = false;
      speedInput.disabled = false;
      clutchInput.disabled = false;
      applyBtn.disabled = false;
    } else {
      toggleBtn.textContent = '🎮 Enable Simulator';
      toggleBtn.classList.remove('btn-danger');
      toggleBtn.classList.add('btn-secondary');

      startBtn.disabled = true;
      stopBtn.disabled = true;
      rpmSlider.disabled = true;
      speedInput.disabled = true;
      clutchInput.disabled = true;
      applyBtn.disabled = true;
    }

    // Update virtual inputs
    if (data.virtualSpeed !== undefined) {
      speedInput.value = data.virtualSpeed;
    }
    if (data.clutchPulled !== undefined) {
      clutchInput.checked = data.clutchPulled;
    }

  } catch (error) {
    console.error('Failed to fetch simulator status:', error);
    document.getElementById('sim-enabled-status').textContent = '❌ Error';
  }
}

// Toggle simulator on/off
async function toggleSimulator() {
  try {
    const action = simulatorEnabled ? 'disable' : 'enable';
    const response = await fetch(`/api/simulator?action=${action}`, { method: 'POST' });
    const data = await response.json();

    if (data.success) {
      showToast(`✅ Simulator ${action}d`, 'success');
      await refreshSimulatorStatus();
    } else {
      showToast('❌ Failed to toggle simulator', 'error');
    }
  } catch (error) {
    console.error('Failed to toggle simulator:', error);
    showToast('❌ Error toggling simulator', 'error');
  }
}

// Start simulator
async function startSimulator() {
  try {
    const rpm = document.getElementById('sim-rpm-slider').value;
    const response = await fetch(`/api/simulator?action=start&rpm=${rpm}`, { method: 'POST' });
    const data = await response.json();

    if (data.success) {
      showToast(`✅ Simulator started at ${rpm} RPM`, 'success');
      await refreshSimulatorStatus();
    } else {
      showToast('❌ Failed to start simulator', 'error');
    }
  } catch (error) {
    console.error('Failed to start simulator:', error);
    showToast('❌ Error starting simulator', 'error');
  }
}

// Stop simulator
async function stopSimulator() {
  try {
    const response = await fetch('/api/simulator?action=stop', { method: 'POST' });
    const data = await response.json();

    if (data.success) {
      showToast('⏹️ Simulator stopped', 'success');
      await refreshSimulatorStatus();
    } else {
      showToast('❌ Failed to stop simulator', 'error');
    }
  } catch (error) {
    console.error('Failed to stop simulator:', error);
    showToast('❌ Error stopping simulator', 'error');
  }
}

// Update RPM display
function updateSimRPMDisplay() {
  const value = document.getElementById('sim-rpm-slider').value;
  document.getElementById('sim-rpm-value').textContent = value;
}

// Apply simulator virtual inputs
async function applySimulatorInputs() {
  try {
    const speed = document.getElementById('sim-speed').value;
    const clutch = document.getElementById('sim-clutch').checked;

    const response = await fetch('/api/simulator?action=setInputs', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        speed: parseInt(speed),
        clutchPulled: clutch
      })
    });

    const data = await response.json();

    if (data.success) {
      showToast('✅ Virtual inputs applied', 'success');
      await refreshSimulatorStatus();
    } else {
      showToast('❌ Failed to apply inputs', 'error');
    }
  } catch (error) {
    console.error('Failed to apply inputs:', error);
    showToast('❌ Error applying inputs', 'error');
  }
}
