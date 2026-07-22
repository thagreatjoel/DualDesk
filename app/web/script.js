// ============================================================
// STATE
// ============================================================
const state = {
    workspaces: [],
    devices: [],
    driverConnected: false,
    lastEvent: 'Ready',
    deviceCount: 0,
    currentView: 'dashboard',
    contextTarget: null
};

const colors = ['#8b5cf6', '#10b981', '#f59e0b', '#ef4444', '#3b82f6'];
const $ = (s) => document.querySelector(s);
const $$ = (s) => document.querySelectorAll(s);

// ============================================================
// CONTEXT MENU
// ============================================================
const contextMenu = document.getElementById('contextMenu');

function showContextMenu(x, y, items) {
    contextMenu.innerHTML = '';
    contextMenu.style.display = 'block';
    contextMenu.style.left = x + 'px';
    contextMenu.style.top = y + 'px';

    items.forEach(item => {
        if (item.divider) {
            const divider = document.createElement('div');
            divider.className = 'context-menu-divider';
            contextMenu.appendChild(divider);
            return;
        }
        const el = document.createElement('div');
        el.className = 'context-menu-item' + (item.danger ? ' danger' : '');
        el.textContent = item.label;
        el.addEventListener('click', () => {
            contextMenu.style.display = 'none';
            if (item.action) item.action();
        });
        contextMenu.appendChild(el);
    });

    // Close on outside click
    setTimeout(() => {
        document.addEventListener('click', closeContextMenu, { once: true });
        document.addEventListener('contextmenu', closeContextMenu, { once: true });
    }, 10);
}

function closeContextMenu() {
    contextMenu.style.display = 'none';
}

// ============================================================
// NAVIGATION
// ============================================================
$$('.nav-item').forEach(el => {
    el.addEventListener('click', () => {
        const view = el.dataset.view;
        state.currentView = view;
        $$('.nav-item').forEach(n => n.classList.remove('active'));
        el.classList.add('active');
        $$('.view').forEach(v => v.classList.remove('active'));
        const target = document.getElementById(`view-${view}`);
        if (target) target.classList.add('active');
        const titles = { dashboard: 'Dashboard', workspaces: 'Workspaces', devices: 'Devices', settings: 'Settings' };
        $('#pageTitle').textContent = titles[view] || 'Dashboard';
    });
});

// ============================================================
// RENDER
// ============================================================
function renderDashboard(data) {
    // Stats
    $('#statWorkspaces').textContent = data.workspaces?.length || 0;
    $('#statDevices').textContent = data.deviceCount || 0;
    $('#statAssigned').textContent = data.devices?.filter(d => d.assigned).length || 0;
    $('#statWindows').textContent = data.workspaces?.reduce((s, w) => s + w.windowCount, 0) || 0;

    // Workspace cards - Dashboard
    const grid = $('#workspaceGrid');
    grid.innerHTML = '';
    if (data.workspaces && data.workspaces.length) {
        data.workspaces.forEach((ws, i) => {
            const card = document.createElement('div');
            card.className = 'workspace-card';
            card.dataset.wsIndex = i;
            card.innerHTML = `
                <div class="workspace-card-header">
                    <div class="workspace-badge" style="background:${colors[i % colors.length]}">${String.fromCharCode(65 + i)}</div>
                    <span class="workspace-name">${ws.name}</span>
                </div>
                <div class="workspace-stats">
                    <span>Windows: ${ws.windowCount}</span>
                    <span>Devices: ${ws.deviceCount}</span>
                </div>
            `;
            // Right-click on workspace
            card.addEventListener('contextmenu', (e) => {
                e.preventDefault();
                showWorkspaceContextMenu(e.clientX, e.clientY, i, data);
            });
            grid.appendChild(card);
        });
    } else {
        grid.innerHTML = '<p style="color:var(--muted);">No workspaces detected.</p>';
    }

    // Workspace cards - Full view
    const fullGrid = $('#workspaceGridFull');
    fullGrid.innerHTML = '';
    if (data.workspaces && data.workspaces.length) {
        data.workspaces.forEach((ws, i) => {
            const card = document.createElement('div');
            card.className = 'workspace-card';
            card.dataset.wsIndex = i;
            card.innerHTML = `
                <div class="workspace-card-header">
                    <div class="workspace-badge" style="background:${colors[i % colors.length]}">${String.fromCharCode(65 + i)}</div>
                    <span class="workspace-name">${ws.name}</span>
                </div>
                <div class="workspace-stats">
                    <span>Windows: ${ws.windowCount}</span>
                    <span>Devices: ${ws.deviceCount}</span>
                </div>
            `;
            card.addEventListener('contextmenu', (e) => {
                e.preventDefault();
                showWorkspaceContextMenu(e.clientX, e.clientY, i, data);
            });
            fullGrid.appendChild(card);
        });
    }

    // Devices
    const list = $('#deviceList');
    list.innerHTML = '';
    if (data.devices && data.devices.length) {
        data.devices.forEach((device, idx) => {
            const item = document.createElement('div');
            item.className = 'device-item';
            item.dataset.deviceIndex = idx;
            const ws = data.workspaces?.find(w => w.id === device.workspaceId);
            const wsName = device.assigned && ws ? ws.name : 'Unassigned';
            const wsClass = device.assigned ? 'assigned' : 'unassigned';
            item.innerHTML = `
                <div class="device-name">
                    <span class="device-dot ${wsClass}"></span>
                    ${device.name}
                </div>
                <div class="device-workspace ${wsClass}">${wsName}</div>
                <div>
                    ${!device.assigned ?
                        data.workspaces?.map(w =>
                            `<button class="btn-sm btn-assign" data-device="${idx}" data-ws="${w.id}">${w.name}</button>`
                        ).join('') || ''
                        :
                        `<button class="btn-sm btn-unassign" data-device="${idx}">Unassign</button>`
                    }
                </div>
            `;
            // Right-click on device
            item.addEventListener('contextmenu', (e) => {
                e.preventDefault();
                showDeviceContextMenu(e.clientX, e.clientY, idx, data);
            });
            list.appendChild(item);
        });

        // Device button click handlers
        list.querySelectorAll('.btn-assign').forEach(btn => {
            btn.addEventListener('click', () => {
                const deviceIdx = parseInt(btn.dataset.device);
                const wsId = parseInt(btn.dataset.ws);
                if (window.chrome?.webview) {
                    window.chrome.webview.postMessage(`assign:${deviceIdx}:${wsId}`);
                }
            });
        });
        list.querySelectorAll('.btn-unassign').forEach(btn => {
            btn.addEventListener('click', () => {
                const deviceIdx = parseInt(btn.dataset.device);
                if (window.chrome?.webview) {
                    window.chrome.webview.postMessage(`unassign:${deviceIdx}`);
                }
            });
        });
    } else {
        list.innerHTML = '<div class="device-item" style="color:var(--muted);">No devices detected.</div>';
    }

    // Status
    const dot = $('#driverDot');
    const statusText = $('#driverStatusText');
    if (data.driverConnected) {
        dot.className = 'status-dot online';
        statusText.textContent = 'Driver: Connected';
    } else {
        dot.className = 'status-dot offline';
        statusText.textContent = 'Driver: Disconnected';
    }

    $('#statusDriver').textContent = `Driver: ${data.driverConnected ? 'Connected' : 'Disconnected'}`;
    $('#statusEvent').textContent = `Event: ${data.lastEvent || 'Ready'}`;
    $('#statusDeviceCount').textContent = `Devices: ${data.deviceCount || 0}`;
    $('#settingsDriverStatus').textContent = data.driverConnected ? 'Connected' : 'Disconnected';
    $('#settingsLastEvent').textContent = data.lastEvent || 'Ready';
}

// ============================================================
// WORKSPACE CONTEXT MENU
// ============================================================
function showWorkspaceContextMenu(x, y, wsIndex, data) {
    const ws = data.workspaces[wsIndex];
    if (!ws) return;

    const items = [
        { label: `${ws.name}`, divider: false },
        { divider: true },
        { label: 'Rename Workspace', action: () => {
            const newName = prompt('Enter new workspace name:', ws.name);
            if (newName && newName.trim()) {
                if (window.chrome?.webview) {
                    window.chrome.webview.postMessage(`rename:${wsIndex}:${newName.trim()}`);
                }
            }
        }},
        { label: 'Assign Device...', action: () => {
            // Switch to devices view and highlight this workspace
            state.currentView = 'devices';
            $$('.nav-item').forEach(n => n.classList.remove('active'));
            document.querySelector('.nav-item[data-view="devices"]')?.classList.add('active');
            $$('.view').forEach(v => v.classList.remove('active'));
            document.getElementById('view-devices')?.classList.add('active');
            $('#pageTitle').textContent = 'Devices';
            // Store selected workspace for device assignment
            state.selectedWorkspace = wsIndex;
        }},
        { divider: true },
        { label: 'Unassign All Devices', danger: true, action: () => {
            if (confirm(`Unassign all devices from ${ws.name}?`)) {
                if (window.chrome?.webview) {
                    window.chrome.webview.postMessage(`unassignAll:${ws.id}`);
                }
            }
        }}
    ];

    // Add device assignment options if devices are available
    const assignedDevices = data.devices?.filter(d => d.assigned && d.workspaceId === ws.id) || [];
    const unassignedDevices = data.devices?.filter(d => !d.assigned) || [];

    if (unassignedDevices.length > 0) {
        items.push({ divider: true });
        items.push({ label: 'Assign Devices:', divider: false });
        unassignedDevices.forEach((d, idx) => {
            const deviceIdx = data.devices.indexOf(d);
            items.push({
                label: `  → ${d.name}`,
                action: () => {
                    if (window.chrome?.webview) {
                        window.chrome.webview.postMessage(`assign:${deviceIdx}:${ws.id}`);
                    }
                }
            });
        });
    }

    showContextMenu(x, y, items);
}

// ============================================================
// DEVICE CONTEXT MENU
// ============================================================
function showDeviceContextMenu(x, y, deviceIdx, data) {
    const device = data.devices[deviceIdx];
    if (!device) return;

    const items = [
        { label: `${device.name}`, divider: false },
        { divider: true }
    ];

    if (device.assigned) {
        const ws = data.workspaces?.find(w => w.id === device.workspaceId);
        items.push({
            label: `Unassign from ${ws ? ws.name : 'Workspace'}`,
            danger: true,
            action: () => {
                if (window.chrome?.webview) {
                    window.chrome.webview.postMessage(`unassign:${deviceIdx}`);
                }
            }
        });
    } else {
        // Show all workspaces to assign to
        if (data.workspaces && data.workspaces.length) {
            items.push({ label: 'Assign to:', divider: false });
            data.workspaces.forEach((ws) => {
                // Check if this device is already assigned to this workspace
                const alreadyAssigned = data.devices?.some(d =>
                    d.assigned && d.workspaceId === ws.id && data.devices.indexOf(d) === deviceIdx
                );
                if (!alreadyAssigned) {
                    items.push({
                        label: `  → ${ws.name}`,
                        action: () => {
                            if (window.chrome?.webview) {
                                window.chrome.webview.postMessage(`assign:${deviceIdx}:${ws.id}`);
                            }
                        }
                    });
                }
            });
        }
    }

    showContextMenu(x, y, items);
}

// ============================================================
// WEBVIEW2 COMMUNICATION
// ============================================================
if (window.chrome?.webview) {
    window.chrome.webview.addEventListener('message', (e) => {
        try {
            const data = JSON.parse(e.data);
            if (data.type === 'refresh') {
                renderDashboard(data.detail);
            }
        } catch (_) {
            // Plain message
        }
    });
}

// ============================================================
// UI EVENTS
// ============================================================
document.querySelector('.btn-refresh')?.addEventListener('click', () => {
    if (window.chrome?.webview) window.chrome.webview.postMessage('refresh');
});

$('#testDriverBtn')?.addEventListener('click', () => {
    if (window.chrome?.webview) window.chrome.webview.postMessage('testDriver');
});

// ============================================================
// INIT
// ============================================================
setTimeout(() => {
    if (window.chrome?.webview) window.chrome.webview.postMessage('refresh');
}, 500);

console.log('DualDesk Web UI loaded.');