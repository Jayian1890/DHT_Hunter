// Dashboard JavaScript

// Global variables for charts
let networkActivityChart;

// Data for charts
const networkActivityData = {
    labels: [],
    bytesReceived: [],
    bytesSent: []
};

// Initialize the dashboard
document.addEventListener('DOMContentLoaded', function() {
    // Initialize charts
    initCharts();

    // Initialize tabs
    initTabs();

    // Initialize settings
    initSettings();

    // Start data polling
    fetchData();
    setInterval(fetchData, 1000);

    // Update uptime
    updateUptime();
    setInterval(updateUptime, 1000);
});

// Initialize tabs
function initTabs() {
    const tabItems = document.querySelectorAll('.tab-item');

    tabItems.forEach(item => {
        item.addEventListener('click', function() {
            // Remove active class from all tabs
            tabItems.forEach(tab => tab.classList.remove('active'));

            // Add active class to clicked tab
            this.classList.add('active');

            // Hide all tab panes
            const tabPanes = document.querySelectorAll('.tab-pane');
            tabPanes.forEach(pane => pane.classList.remove('active'));

            // Show the selected tab pane
            const tabName = this.getAttribute('data-tab');
            document.getElementById(tabName + '-tab').classList.add('active');
        });
    });
}

// Initialize charts
function initCharts() {
    // Network Activity Chart - Now showing network speed over 24 hours
    const networkCtx = document.getElementById('network-activity-chart').getContext('2d');
    networkActivityChart = new Chart(networkCtx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                {
                    label: 'Download Speed (KB/s)',
                    data: [],
                    borderColor: '#3498db',
                    backgroundColor: 'rgba(52, 152, 219, 0.1)',
                    borderWidth: 2,
                    tension: 0.4,
                    fill: true
                },
                {
                    label: 'Upload Speed (KB/s)',
                    data: [],
                    borderColor: '#2ecc71',
                    backgroundColor: 'rgba(46, 204, 113, 0.1)',
                    borderWidth: 2,
                    tension: 0.4,
                    fill: true
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                y: {
                    beginAtZero: true,
                    title: {
                        display: true,
                        text: 'Speed (KB/s)'
                    }
                },
                x: {
                    title: {
                        display: true,
                        text: 'Time (last 24 hours)'
                    }
                }
            },
            animation: {
                duration: 500
            },
            plugins: {
                legend: {
                    position: 'top'
                },
                title: {
                    display: true,
                    text: 'Network Speed (24 Hours)'
                }
            }
        }
    });

    // Node Distribution Chart has been removed
}

// Fetch data from API
function fetchData() {
    // Fetch statistics
    fetch('/api/statistics')
        .then(response => response.json())
        .then(data => {
            updateStatistics(data);
            updateNetworkActivityChart(data);
        })
        .catch(error => {
            console.error('Error fetching statistics:', error);
            updateStatus(false);
        });

    // Fetch nodes
    fetch('/api/nodes?limit=10')
        .then(response => response.json())
        .then(data => {
            updateNodesTable(data);
        })
        .catch(error => {
            console.error('Error fetching nodes:', error);
        });

    // Fetch info hashes
    fetch('/api/infohashes?limit=10')
        .then(response => response.json())
        .then(data => {
            updateInfoHashesTable(data);
        })
        .catch(error => {
            console.error('Error fetching info hashes:', error);
        });
}

// Update statistics display
function updateStatistics(data) {
    document.getElementById('nodes-discovered').textContent = data.nodesDiscovered.toLocaleString();
    document.getElementById('nodes-in-table').textContent = data.nodesInTable.toLocaleString();
    document.getElementById('peers-discovered').textContent = data.peersDiscovered.toLocaleString();
    document.getElementById('messages-received').textContent = data.messagesReceived.toLocaleString();
    document.getElementById('messages-sent').textContent = data.messagesSent.toLocaleString();
    document.getElementById('info-hashes').textContent = data.infoHashes.toLocaleString();

    updateStatus(true);
}

// Update network activity chart
function updateNetworkActivityChart(data) {
    const now = new Date();
    const timeLabel = now.getHours().toString().padStart(2, '0') + ':' +
                     now.getMinutes().toString().padStart(2, '0');

    // Add new data
    // For 24-hour display, we'll get data from the API
    // This assumes the API now returns bytesReceived and bytesSent arrays for the last 24 hours
    if (data.networkSpeed && data.networkSpeed.labels) {
        networkActivityData.labels = data.networkSpeed.labels;
        networkActivityData.bytesReceived = data.networkSpeed.bytesReceived || [];
        networkActivityData.bytesSent = data.networkSpeed.bytesSent || [];
    }

    // Update chart
    networkActivityChart.data.labels = networkActivityData.labels;
    networkActivityChart.data.datasets[0].data = networkActivityData.bytesReceived;
    networkActivityChart.data.datasets[1].data = networkActivityData.bytesSent;
    networkActivityChart.update();
}

// Node distribution chart has been removed

// Update nodes table
function updateNodesTable(nodes) {
    const tbody = document.querySelector('#nodes-table tbody');
    tbody.innerHTML = '';

    nodes.forEach(node => {
        const tr = document.createElement('tr');

        const nodeIdTd = document.createElement('td');
        nodeIdTd.textContent = formatNodeId(node.nodeId);
        nodeIdTd.title = node.nodeId; // Show full node ID on hover
        tr.appendChild(nodeIdTd);

        const ipTd = document.createElement('td');
        ipTd.textContent = node.ip;
        tr.appendChild(ipTd);

        const portTd = document.createElement('td');
        portTd.textContent = node.port;
        tr.appendChild(portTd);

        const lastSeenTd = document.createElement('td');
        lastSeenTd.textContent = formatTimeAgo(node.lastSeen);
        lastSeenTd.title = new Date(node.lastSeen).toLocaleString(); // Show exact time on hover
        tr.appendChild(lastSeenTd);

        // Add status column
        const statusTd = document.createElement('td');
        const timeSinceLastSeen = Date.now() - node.lastSeen;

        if (timeSinceLastSeen < 300000) { // Less than 5 minutes
            statusTd.textContent = 'Active';
            statusTd.className = 'status-complete';
        } else if (timeSinceLastSeen < 3600000) { // Less than 1 hour
            statusTd.textContent = 'Recent';
            statusTd.className = 'status-in-progress';
        } else {
            statusTd.textContent = 'Inactive';
            statusTd.className = 'status-failed';
        }

        tr.appendChild(statusTd);

        tbody.appendChild(tr);
    });
}

// Update info hashes table
function updateInfoHashesTable(infoHashes) {
    const tbody = document.querySelector('#info-hashes-table tbody');
    tbody.innerHTML = '';

    infoHashes.forEach(infoHash => {
        const tr = document.createElement('tr');

        // Info Hash
        const hashTd = document.createElement('td');
        hashTd.textContent = formatInfoHash(infoHash.hash);
        hashTd.title = infoHash.hash; // Show full hash on hover
        tr.appendChild(hashTd);

        // Name
        const nameTd = document.createElement('td');
        nameTd.textContent = infoHash.name || 'Unknown';
        tr.appendChild(nameTd);

        // Peers
        const peersTd = document.createElement('td');
        peersTd.textContent = infoHash.peers.toLocaleString();
        tr.appendChild(peersTd);

        // Size
        const sizeTd = document.createElement('td');
        sizeTd.textContent = infoHash.size ? formatSize(infoHash.size) : 'Unknown';
        tr.appendChild(sizeTd);

        // Files
        const filesTd = document.createElement('td');
        filesTd.textContent = infoHash.fileCount ? infoHash.fileCount.toLocaleString() : 'Unknown';
        tr.appendChild(filesTd);

        // First Seen
        const firstSeenTd = document.createElement('td');
        firstSeenTd.textContent = formatTimeAgo(infoHash.firstSeen);
        firstSeenTd.title = new Date(infoHash.firstSeen).toLocaleString(); // Show exact time on hover
        tr.appendChild(firstSeenTd);

        // Metadata Status
        const statusTd = document.createElement('td');
        if (infoHash.metadataStatus) {
            statusTd.textContent = infoHash.metadataStatus;

            // Add status styling
            if (infoHash.metadataStatus === 'Queued') {
                statusTd.className = 'status-queued';
            } else if (infoHash.metadataStatus === 'In Progress') {
                statusTd.className = 'status-in-progress';
            } else if (infoHash.metadataStatus === 'Complete') {
                statusTd.className = 'status-complete';
            } else if (infoHash.metadataStatus.includes('Failed')) {
                statusTd.className = 'status-failed';
            } else if (infoHash.metadataStatus.includes('Retry')) {
                statusTd.className = 'status-retry';
            }
        } else {
            statusTd.textContent = 'Not Started';
            statusTd.className = 'status-not-started';
        }
        tr.appendChild(statusTd);

        // Actions column
        const actionsTd = document.createElement('td');

        // Add a button to acquire metadata if not complete
        if (!infoHash.metadataStatus || !infoHash.metadataStatus.includes('Complete')) {
            const acquireBtn = document.createElement('button');
            acquireBtn.textContent = 'Acquire Metadata';
            acquireBtn.className = 'acquire-btn';
            acquireBtn.onclick = function() {
                acquireMetadata(infoHash.hash);
            };
            actionsTd.appendChild(acquireBtn);
        }

        // Add a details button
        const detailsBtn = document.createElement('button');
        detailsBtn.textContent = 'Details';
        detailsBtn.className = 'details-btn';
        detailsBtn.onclick = function() {
            alert('Info Hash Details:\n' +
                  'Hash: ' + infoHash.hash + '\n' +
                  'Name: ' + (infoHash.name || 'Unknown') + '\n' +
                  'Peers: ' + infoHash.peers + '\n' +
                  'Size: ' + (infoHash.size ? formatSize(infoHash.size) : 'Unknown') + '\n' +
                  'Files: ' + (infoHash.fileCount || 'Unknown') + '\n' +
                  'First Seen: ' + new Date(infoHash.firstSeen).toLocaleString() + '\n' +
                  'Status: ' + (infoHash.metadataStatus || 'Not Started'));
        };
        actionsTd.appendChild(detailsBtn);

        tr.appendChild(actionsTd);

        tbody.appendChild(tr);
    });
}

// Update status indicator
function updateStatus(isActive) {
    const indicator = document.getElementById('status-indicator');
    const text = document.getElementById('status-text');

    if (isActive) {
        indicator.className = 'status-active';
        text.textContent = 'Active';
    } else {
        indicator.className = 'status-active status-inactive';
        text.textContent = 'Inactive';
    }
}

// Update uptime
function updateUptime() {
    fetch('/api/uptime')
        .then(response => response.json())
        .then(data => {
            const uptime = data.uptime;
            const days = Math.floor(uptime / 86400);
            const hours = Math.floor((uptime % 86400) / 3600);
            const minutes = Math.floor((uptime % 3600) / 60);
            const seconds = Math.floor(uptime % 60);

            document.getElementById('uptime').textContent =
                `Uptime: ${days}d ${hours}h ${minutes}m ${seconds}s`;
        })
        .catch(error => {
            console.error('Error fetching uptime:', error);
        });
}

// Helper function to format node ID
function formatNodeId(nodeId) {
    if (nodeId.length > 16) {
        return nodeId.substring(0, 6) + '...' + nodeId.substring(nodeId.length - 6);
    }
    return nodeId;
}

// Helper function to format info hash
function formatInfoHash(hash) {
    if (hash.length > 16) {
        return hash.substring(0, 6) + '...' + hash.substring(hash.length - 6);
    }
    return hash;
}

// Helper function to format file size
function formatSize(bytes) {
    if (bytes === 0) return '0 Bytes';

    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));

    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

// Helper function to format time ago
function formatTimeAgo(timestamp) {
    const now = Date.now();
    const diff = now - timestamp;

    if (diff < 60000) {
        return 'Just now';
    } else if (diff < 3600000) {
        return Math.floor(diff / 60000) + ' min ago';
    } else if (diff < 86400000) {
        return Math.floor(diff / 3600000) + ' hours ago';
    } else {
        return Math.floor(diff / 86400000) + ' days ago';
    }
}

// Function to acquire metadata for an InfoHash
function acquireMetadata(infoHash) {
    fetch('/api/metadata/acquire', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({
            infoHash: infoHash
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            console.log('Metadata acquisition started for', infoHash);
            // Refresh the data immediately
            fetchData();
        } else {
            console.error('Failed to start metadata acquisition:', data.error);
            alert('Failed to start metadata acquisition: ' + data.error);
        }
    })
    .catch(error => {
        console.error('Error starting metadata acquisition:', error);
        alert('Error starting metadata acquisition: ' + error);
    });
}

// Initialize settings panel
function initSettings() {
    // Load settings when the settings tab is clicked
    document.querySelector('.tab-item[data-tab="settings"]').addEventListener('click', loadSettings);

    // Add event listeners for save and reload buttons
    document.getElementById('save-settings').addEventListener('click', saveSettings);
    document.getElementById('reload-settings').addEventListener('click', loadSettings);
}

// Load settings from the server
function loadSettings() {
    fetch('/api/config')
        .then(response => response.json())
        .then(data => {
            // Clear existing settings
            document.getElementById('general-settings').innerHTML = '';
            document.getElementById('network-settings').innerHTML = '';
            document.getElementById('web-settings').innerHTML = '';
            document.getElementById('crawler-settings').innerHTML = '';
            document.getElementById('persistence-settings').innerHTML = '';

            // Populate settings groups
            if (data.general) {
                populateSettingsGroup('general', data.general);
            }

            if (data.network) {
                populateSettingsGroup('network', data.network);
            }

            if (data.web) {
                populateSettingsGroup('web', data.web);
            }

            if (data.crawler) {
                populateSettingsGroup('crawler', data.crawler);
            }

            if (data.persistence) {
                populateSettingsGroup('persistence', data.persistence);
            }
        })
        .catch(error => {
            console.error('Error loading settings:', error);
            alert('Error loading settings: ' + error);
        });
}

// Populate a settings group with settings
function populateSettingsGroup(group, settings) {
    const container = document.getElementById(group + '-settings');

    for (const key in settings) {
        const value = settings[key];
        const settingItem = document.createElement('div');
        settingItem.className = 'setting-item';

        const label = document.createElement('label');
        label.className = 'setting-label';
        label.textContent = formatSettingName(key);
        settingItem.appendChild(label);

        const description = document.createElement('div');
        description.className = 'setting-description';
        description.textContent = getSettingDescription(group, key);
        settingItem.appendChild(description);

        // Create appropriate input based on value type
        if (typeof value === 'boolean') {
            const input = document.createElement('input');
            input.type = 'checkbox';
            input.className = 'setting-checkbox';
            input.checked = value;
            input.id = `setting-${group}-${key}`;
            input.dataset.group = group;
            input.dataset.key = key;
            input.dataset.type = 'boolean';
            settingItem.appendChild(input);
        } else if (typeof value === 'number') {
            const input = document.createElement('input');
            input.type = 'number';
            input.className = 'setting-input';
            input.value = value;
            input.id = `setting-${group}-${key}`;
            input.dataset.group = group;
            input.dataset.key = key;
            input.dataset.type = 'number';
            settingItem.appendChild(input);
        } else {
            const input = document.createElement('input');
            input.type = 'text';
            input.className = 'setting-input';
            input.value = value;
            input.id = `setting-${group}-${key}`;
            input.dataset.group = group;
            input.dataset.key = key;
            input.dataset.type = 'string';
            settingItem.appendChild(input);
        }

        container.appendChild(settingItem);
    }
}

// Format setting name for display
function formatSettingName(key) {
    return key
        .replace(/([A-Z])/g, ' $1') // Add space before capital letters
        .replace(/^./, str => str.toUpperCase()) // Capitalize first letter
        .trim();
}

// Get setting description
function getSettingDescription(group, key) {
    const descriptions = {
        general: {
            configDir: 'Directory for configuration files',
            logFile: 'Log file name',
            logLevel: 'Logging level (trace, debug, info, warning, error)',
        },
        network: {
            transactionTimeout: 'Timeout for DHT transactions in seconds',
            maxTransactions: 'Maximum number of concurrent transactions',
            mtuSize: 'Maximum transmission unit size in bytes',
        },
        web: {
            port: 'Web interface port',
            webRoot: 'Web interface root directory',
        },
        crawler: {
            parallelCrawls: 'Number of parallel crawl operations',
            refreshInterval: 'Refresh interval in minutes',
            maxNodes: 'Maximum number of nodes to track',
            maxInfoHashes: 'Maximum number of info hashes to track',
            autoStart: 'Automatically start crawler on startup',
        },
        persistence: {
            saveInterval: 'Save interval in minutes',
            routingTablePath: 'Path to routing table data file',
            peerStoragePath: 'Path to peer storage data file',
            metadataPath: 'Path to metadata data file',
            nodeIDPath: 'Path to node ID data file',
        },
    };

    return descriptions[group] && descriptions[group][key]
        ? descriptions[group][key]
        : `Setting for ${key}`;
}

// Save settings to the server
function saveSettings() {
    const settings = {};

    // Collect all settings
    document.querySelectorAll('[id^="setting-"]').forEach(input => {
        const group = input.dataset.group;
        const key = input.dataset.key;
        const type = input.dataset.type;

        if (!settings[group]) {
            settings[group] = {};
        }

        if (type === 'boolean') {
            settings[group][key] = input.checked;
        } else if (type === 'number') {
            settings[group][key] = parseFloat(input.value);
        } else {
            settings[group][key] = input.value;
        }
    });

    // Save each group
    for (const group in settings) {
        const settingsJson = JSON.stringify(settings[group]);

        fetch(`/api/config/${group}`, {
            method: 'PUT',
            headers: {
                'Content-Type': 'application/json'
            },
            body: settingsJson
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                console.log(`Settings for ${group} saved successfully`);
            } else {
                console.error(`Failed to save settings for ${group}:`, data.error);
                alert(`Failed to save settings for ${group}: ${data.error}`);
            }
        })
        .catch(error => {
            console.error(`Error saving settings for ${group}:`, error);
            alert(`Error saving settings for ${group}: ${error}`);
        });
    }

    // Save configuration to disk
    fetch('/api/config/save', {
        method: 'POST',
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            alert('Settings saved successfully');
        } else {
            console.error('Failed to save configuration to disk:', data.error);
            alert('Settings updated but failed to save to disk: ' + data.error);
        }
    })
    .catch(error => {
        console.error('Error saving configuration to disk:', error);
        alert('Error saving configuration to disk: ' + error);
    });
}
