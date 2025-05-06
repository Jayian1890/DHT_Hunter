// Dashboard JavaScript

// Global variables
// No charts needed anymore

// Global variables for logs
let currentLogPage = 1;
let logsPerPage = 50;
let currentLogSeverity = 'info';
let currentLogSource = '';
let autoRefreshLogs = true;

// Initialize the dashboard
document.addEventListener('DOMContentLoaded', function() {
    // Initialize tabs
    initTabs();

    // Initialize settings
    initSettings();

    // Initialize logs
    initLogs();

    // Start data polling
    fetchData();
    setInterval(fetchData, 1000);

    // Update uptime
    updateUptime();
    setInterval(updateUptime, 1000);

    // Auto-refresh logs
    setInterval(function() {
        if (autoRefreshLogs && document.getElementById('logs-tab').classList.contains('active')) {
            fetchLogs();
        }
    }, 5000); // Refresh logs every 5 seconds
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

// Charts have been removed

// Initialize logs functionality
function initLogs() {
    // Set up event listeners for log controls
    document.getElementById('log-severity').addEventListener('change', function() {
        currentLogSeverity = this.value;
        fetchLogs();
    });

    document.getElementById('log-source').addEventListener('input', function() {
        currentLogSource = this.value;
        fetchLogs();
    });

    document.getElementById('refresh-logs').addEventListener('click', function() {
        fetchLogs();
    });

    document.getElementById('clear-logs').addEventListener('click', function() {
        document.querySelector('#logs-table tbody').innerHTML = '';
    });

    document.getElementById('load-more-logs').addEventListener('click', function() {
        currentLogPage++;
        fetchLogs(true); // Append mode
    });

    // Initial fetch
    fetchLogs();
}

// Fetch logs from API
function fetchLogs(append = false) {
    const limit = logsPerPage;
    const offset = append ? (currentLogPage - 1) * logsPerPage : 0;

    let url = `/api/logs?limit=${limit}`;
    if (currentLogSeverity) {
        url += `&severity=${currentLogSeverity}`;
    }
    if (currentLogSource) {
        url += `&source=${encodeURIComponent(currentLogSource)}`;
    }

    fetch(url)
        .then(response => response.json())
        .then(data => {
            updateLogsTable(data, append);
        })
        .catch(error => {
            console.error('Error fetching logs:', error);
        });
}

// Update logs table
function updateLogsTable(data, append = false) {
    const tbody = document.querySelector('#logs-table tbody');

    // Clear table if not in append mode
    if (!append) {
        tbody.innerHTML = '';
    }

    // Check if we have logs
    if (!data.logs || data.logs.length === 0) {
        if (!append) {
            const tr = document.createElement('tr');
            tr.innerHTML = `<td colspan="4" class="no-data">No logs found</td>`;
            tbody.appendChild(tr);
        }
        return;
    }

    // Add logs to table
    data.logs.forEach(log => {
        const tr = document.createElement('tr');

        // Format timestamp
        const timestamp = new Date(log.timestamp);
        const formattedTime = timestamp.toLocaleTimeString();

        tr.innerHTML = `
            <td>${formattedTime}</td>
            <td class="log-${log.severity.toLowerCase()}">${log.severity}</td>
            <td>${log.source}</td>
            <td>${log.formattedMessage}</td>
        `;

        tbody.appendChild(tr);
    });

    // Scroll to bottom if not in append mode
    if (!append) {
        const logWindow = document.querySelector('.log-window');
        logWindow.scrollTop = logWindow.scrollHeight;
    }
}

// Fetch data from API
function fetchData() {
    // Fetch statistics
    fetch('/api/statistics')
        .then(response => response.json())
        .then(data => {
            updateStatistics(data);
        })
        .catch(error => {
            console.error('Error fetching statistics:', error);
            updateStatus(false);
        });
}

// Update statistics display
function updateStatistics(data) {
    // Update nodes count - only show nodes in table as 'Total Nodes'
    document.getElementById('nodes-in-table').textContent = data.nodesInTable.toLocaleString();

    // Update peers count
    document.getElementById('peers-discovered').textContent = data.peersDiscovered.toLocaleString();

    // Update messages count
    document.getElementById('messages-received').textContent = data.messagesReceived.toLocaleString();
    document.getElementById('messages-sent').textContent = data.messagesSent.toLocaleString();

    // Update info hashes count
    document.getElementById('info-hashes').textContent = data.infoHashes.toLocaleString();

    // Update current network speed if available
    if (data.networkSpeed) {
        if (data.networkSpeed.downloadSpeed) {
            const downloadSpeed = data.networkSpeed.downloadSpeed || 0;
            document.getElementById('current-download').textContent = downloadSpeed.toFixed(2) + ' KB/s';
        }

        if (data.networkSpeed.uploadSpeed) {
            const uploadSpeed = data.networkSpeed.uploadSpeed || 0;
            document.getElementById('current-upload').textContent = uploadSpeed.toFixed(2) + ' KB/s';
        }
    }

    updateStatus(true);
}

// Network activity chart has been removed

// Node distribution chart has been removed

// Nodes table has been removed

// Update info hashes table
function updateInfoHashesTable(infoHashes) {
    const tbody = document.querySelector('#info-hashes-table tbody');
    tbody.innerHTML = '';

    infoHashes.forEach(infoHash => {
        const tr = document.createElement('tr');

        // Info Hash - make it clickable
        const hashTd = document.createElement('td');
        const hashLink = document.createElement('a');
        hashLink.href = `infohash.html?hash=${infoHash.hash}`;
        hashLink.textContent = formatInfoHash(infoHash.hash);
        hashLink.title = `View details for ${infoHash.hash}`;
        hashLink.className = 'info-hash-link';
        hashTd.appendChild(hashLink);
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

        // Set tooltip only if timestamp is valid
        if (infoHash.firstSeen && infoHash.firstSeen > 0 && infoHash.firstSeen <= Date.now() * 10) {
            firstSeenTd.title = new Date(infoHash.firstSeen).toLocaleString(); // Show exact time on hover
        } else {
            firstSeenTd.title = 'Unknown timestamp';
        }

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
        detailsBtn.innerHTML = '<i class="fas fa-info-circle"></i> Details';
        detailsBtn.className = 'details-btn';
        detailsBtn.onclick = function() {
            // Format first seen date
            let firstSeenStr = 'Unknown';
            if (infoHash.firstSeen && infoHash.firstSeen > 0 && infoHash.firstSeen <= Date.now() * 10) {
                firstSeenStr = new Date(infoHash.firstSeen).toLocaleString();
            }

            alert('Info Hash Details:\n' +
                  'Hash: ' + infoHash.hash + '\n' +
                  'Name: ' + (infoHash.name || 'Unknown') + '\n' +
                  'Peers: ' + infoHash.peers + '\n' +
                  'Size: ' + (infoHash.size ? formatSize(infoHash.size) : 'Unknown') + '\n' +
                  'Files: ' + (infoHash.fileCount || 'Unknown') + '\n' +
                  'First Seen: ' + firstSeenStr + '\n' +
                  'Status: ' + (infoHash.metadataStatus || 'Not Started'));
        };
        actionsTd.appendChild(detailsBtn);

        // Add a view peers button
        if (infoHash.peers > 0) {
            const peersBtn = document.createElement('button');
            peersBtn.innerHTML = '<i class="fas fa-users"></i> Peers';
            peersBtn.className = 'peers-btn';
            peersBtn.onclick = function() {
                window.location.href = 'peers.html?infoHash=' + encodeURIComponent(infoHash.hash);
            };
            actionsTd.appendChild(peersBtn);
        }

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
    // Check if timestamp is valid
    if (!timestamp || timestamp <= 0 || timestamp > Date.now() * 10) {
        return 'Unknown';
    }

    const now = Date.now();
    const diff = now - timestamp;

    if (diff < 60000) {
        return 'Just now';
    } else if (diff < 3600000) {
        return Math.floor(diff / 60000) + ' min ago';
    } else if (diff < 86400000) {
        return Math.floor(diff / 3600000) + ' hours ago';
    } else {
        // Limit to 30 days to avoid showing extremely old dates
        const days = Math.floor(diff / 86400000);
        return (days > 30 ? '30+' : days) + ' days ago';
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

        // Special handling for known settings

        // General settings
        if (group === 'general' && (key === 'configDir' || key === 'logFile' || key === 'logLevel')) {
            // These should always be strings
            const input = document.createElement('input');
            input.type = 'text';
            input.className = 'setting-input';
            input.value = String(value);
            input.id = `setting-${group}-${key}`;
            input.dataset.group = group;
            input.dataset.key = key;
            input.dataset.type = 'string';
            settingItem.appendChild(input);
        }
        // Web settings
        else if (group === 'web' && key === 'port') {
            // Web port should always be a number
            const input = document.createElement('input');
            input.type = 'number';
            input.className = 'setting-input';
            input.value = Number(value);
            input.min = '1';
            input.max = '65535';
            input.id = `setting-${group}-${key}`;
            input.dataset.group = group;
            input.dataset.key = key;
            input.dataset.type = 'number';
            settingItem.appendChild(input);
        } else if (group === 'web' && key === 'webRoot') {
            // webRoot should always be a string
            const input = document.createElement('input');
            input.type = 'text';
            input.className = 'setting-input';
            input.value = String(value);
            input.id = `setting-${group}-${key}`;
            input.dataset.group = group;
            input.dataset.key = key;
            input.dataset.type = 'string';
            settingItem.appendChild(input);
        } else if (group === 'web' && key === 'useBundledFiles') {
            // useBundledFiles should always be a boolean
            const input = document.createElement('input');
            input.type = 'checkbox';
            input.className = 'setting-checkbox';
            input.checked = Boolean(value);
            input.id = `setting-${group}-${key}`;
            input.dataset.group = group;
            input.dataset.key = key;
            input.dataset.type = 'boolean';
            settingItem.appendChild(input);
        } else if (typeof value === 'boolean') {
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
            useBundledFiles: 'Use bundled web files instead of files from disk',
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

    // Ensure settings have the correct types

    // General settings
    if (settings.general) {
        // Ensure configDir is a string
        if (settings.general.configDir !== undefined) {
            settings.general.configDir = String(settings.general.configDir);
        }

        // Ensure logFile is a string
        if (settings.general.logFile !== undefined) {
            settings.general.logFile = String(settings.general.logFile);
        }

        // Ensure logLevel is a string
        if (settings.general.logLevel !== undefined) {
            settings.general.logLevel = String(settings.general.logLevel);
        }
    }

    // Web settings
    if (settings.web) {
        // Ensure port is a number
        if (settings.web.port !== undefined) {
            settings.web.port = parseInt(settings.web.port, 10);
            if (isNaN(settings.web.port)) {
                alert('Web port must be a valid number');
                return;
            }
        }

        // Ensure webRoot is a string
        if (settings.web.webRoot !== undefined) {
            settings.web.webRoot = String(settings.web.webRoot);
        }

        // Ensure useBundledFiles is a boolean if it exists
        if (settings.web.useBundledFiles !== undefined) {
            settings.web.useBundledFiles = Boolean(settings.web.useBundledFiles);
        }
    }

    // Network settings
    if (settings.network) {
        // Ensure transactionTimeout is a number
        if (settings.network.transactionTimeout !== undefined) {
            settings.network.transactionTimeout = parseInt(settings.network.transactionTimeout, 10);
        }

        // Ensure maxTransactions is a number
        if (settings.network.maxTransactions !== undefined) {
            settings.network.maxTransactions = parseInt(settings.network.maxTransactions, 10);
        }

        // Ensure mtuSize is a number
        if (settings.network.mtuSize !== undefined) {
            settings.network.mtuSize = parseInt(settings.network.mtuSize, 10);
        }
    }

    // Crawler settings
    if (settings.crawler) {
        // Ensure parallelCrawls is a number
        if (settings.crawler.parallelCrawls !== undefined) {
            settings.crawler.parallelCrawls = parseInt(settings.crawler.parallelCrawls, 10);
        }

        // Ensure refreshInterval is a number
        if (settings.crawler.refreshInterval !== undefined) {
            settings.crawler.refreshInterval = parseInt(settings.crawler.refreshInterval, 10);
        }

        // Ensure maxNodes is a number
        if (settings.crawler.maxNodes !== undefined) {
            settings.crawler.maxNodes = parseInt(settings.crawler.maxNodes, 10);
        }

        // Ensure maxInfoHashes is a number
        if (settings.crawler.maxInfoHashes !== undefined) {
            settings.crawler.maxInfoHashes = parseInt(settings.crawler.maxInfoHashes, 10);
        }

        // Ensure autoStart is a boolean
        if (settings.crawler.autoStart !== undefined) {
            settings.crawler.autoStart = Boolean(settings.crawler.autoStart);
        }
    }

    // Persistence settings
    if (settings.persistence) {
        // Ensure saveInterval is a number
        if (settings.persistence.saveInterval !== undefined) {
            settings.persistence.saveInterval = parseInt(settings.persistence.saveInterval, 10);
        }

        // Ensure all paths are strings
        const pathKeys = ['routingTablePath', 'peerStoragePath', 'metadataPath', 'nodeIDPath'];
        pathKeys.forEach(key => {
            if (settings.persistence[key] !== undefined) {
                settings.persistence[key] = String(settings.persistence[key]);
            }
        });
    }

    // Save each group
    for (const group in settings) {
        try {
            // Validate settings before saving
            const settingsObj = settings[group];

            // Ensure all values have the correct types
            for (const key in settingsObj) {
                const value = settingsObj[key];
                if (typeof value === 'number' && isNaN(value)) {
                    alert(`Invalid number value for ${group}.${key}`);
                    return;
                }
            }

            const settingsJson = JSON.stringify(settingsObj);
            console.log(`Saving ${group} settings:`, settingsJson);

            fetch(`/api/config/${group}`, {
                method: 'PUT',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: settingsJson
            })
            .then(response => {
                if (!response.ok) {
                    return response.text().then(text => {
                        try {
                            // Try to parse as JSON
                            const errorData = JSON.parse(text);
                            throw new Error(errorData.error || `HTTP error ${response.status}`);
                        } catch (e) {
                            // If not valid JSON, use the raw text
                            throw new Error(`HTTP error ${response.status}: ${text}`);
                        }
                    });
                }
                return response.json();
            })
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
        } catch (error) {
            console.error(`Error preparing settings for ${group}:`, error);
            alert(`Error preparing settings for ${group}: ${error}`);
        }
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
