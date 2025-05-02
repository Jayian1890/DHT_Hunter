// Dashboard JavaScript

// Global variables for charts
let networkActivityChart;
let nodeDistributionChart;

// Data for charts
const networkActivityData = {
    labels: [],
    messagesReceived: [],
    messagesSent: []
};

// Initialize the dashboard
document.addEventListener('DOMContentLoaded', function() {
    // Initialize charts
    initCharts();

    // Start data polling
    fetchData();
    setInterval(fetchData, 1000);

    // Update uptime
    updateUptime();
    setInterval(updateUptime, 1000);
});

// Initialize charts
function initCharts() {
    // Network Activity Chart
    const networkCtx = document.getElementById('network-activity-chart').getContext('2d');
    networkActivityChart = new Chart(networkCtx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                {
                    label: 'Messages Received',
                    data: [],
                    borderColor: '#3498db',
                    backgroundColor: 'rgba(52, 152, 219, 0.1)',
                    borderWidth: 2,
                    tension: 0.4,
                    fill: true
                },
                {
                    label: 'Messages Sent',
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
                    beginAtZero: true
                }
            },
            animation: {
                duration: 500
            },
            plugins: {
                legend: {
                    position: 'top'
                }
            }
        }
    });

    // Node Distribution Chart
    const nodeCtx = document.getElementById('node-distribution-chart').getContext('2d');
    nodeDistributionChart = new Chart(nodeCtx, {
        type: 'doughnut',
        data: {
            labels: ['Discovered', 'In Routing Table'],
            datasets: [{
                data: [0, 0],
                backgroundColor: [
                    'rgba(52, 152, 219, 0.7)',
                    'rgba(46, 204, 113, 0.7)'
                ],
                borderColor: [
                    'rgba(52, 152, 219, 1)',
                    'rgba(46, 204, 113, 1)'
                ],
                borderWidth: 1
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    position: 'top'
                }
            },
            animation: {
                duration: 500
            }
        }
    });
}

// Fetch data from API
function fetchData() {
    // Fetch statistics
    fetch('/api/statistics')
        .then(response => response.json())
        .then(data => {
            updateStatistics(data);
            updateNetworkActivityChart(data);
            updateNodeDistributionChart(data);
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
                     now.getMinutes().toString().padStart(2, '0') + ':' +
                     now.getSeconds().toString().padStart(2, '0');

    // Add new data
    networkActivityData.labels.push(timeLabel);
    networkActivityData.messagesReceived.push(data.messagesReceived);
    networkActivityData.messagesSent.push(data.messagesSent);

    // Keep only the last 30 data points
    if (networkActivityData.labels.length > 30) {
        networkActivityData.labels.shift();
        networkActivityData.messagesReceived.shift();
        networkActivityData.messagesSent.shift();
    }

    // Update chart
    networkActivityChart.data.labels = networkActivityData.labels;
    networkActivityChart.data.datasets[0].data = networkActivityData.messagesReceived;
    networkActivityChart.data.datasets[1].data = networkActivityData.messagesSent;
    networkActivityChart.update();
}

// Update node distribution chart
function updateNodeDistributionChart(data) {
    nodeDistributionChart.data.datasets[0].data = [
        data.nodesDiscovered - data.nodesInTable,
        data.nodesInTable
    ];
    nodeDistributionChart.update();
}

// Update nodes table
function updateNodesTable(nodes) {
    const tbody = document.querySelector('#nodes-table tbody');
    tbody.innerHTML = '';

    nodes.forEach(node => {
        const tr = document.createElement('tr');

        const nodeIdTd = document.createElement('td');
        nodeIdTd.textContent = formatNodeId(node.nodeId);
        tr.appendChild(nodeIdTd);

        const ipTd = document.createElement('td');
        ipTd.textContent = node.ip;
        tr.appendChild(ipTd);

        const portTd = document.createElement('td');
        portTd.textContent = node.port;
        tr.appendChild(portTd);

        const lastSeenTd = document.createElement('td');
        lastSeenTd.textContent = formatTimeAgo(node.lastSeen);
        tr.appendChild(lastSeenTd);

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

            // Add a button to acquire metadata
            const acquireBtn = document.createElement('button');
            acquireBtn.textContent = 'Acquire';
            acquireBtn.className = 'acquire-btn';
            acquireBtn.onclick = function() {
                acquireMetadata(infoHash.hash);
            };
            statusTd.appendChild(acquireBtn);
        }
        tr.appendChild(statusTd);

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
