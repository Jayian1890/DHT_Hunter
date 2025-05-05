// Info Hash Detail page JavaScript

// Initialize the page
document.addEventListener('DOMContentLoaded', function() {
    // Get the info hash from the URL
    const urlParams = new URLSearchParams(window.location.search);
    const infoHash = urlParams.get('hash');
    
    if (!infoHash) {
        // No info hash provided, redirect to dashboard
        window.location.href = 'index.html';
        return;
    }
    
    // Display the info hash
    document.getElementById('info-hash').textContent = infoHash;
    
    // Set up the view peers link
    const viewPeersLink = document.getElementById('view-peers-link');
    viewPeersLink.href = `peers.html?infoHash=${infoHash}`;
    
    // Set up the acquire metadata button
    const acquireMetadataBtn = document.getElementById('acquire-metadata-btn');
    acquireMetadataBtn.addEventListener('click', function() {
        acquireMetadata(infoHash);
    });
    
    // Fetch info hash details
    fetchInfoHashDetails(infoHash);
    
    // Update uptime
    updateUptime();
    setInterval(updateUptime, 1000);
    
    // Refresh data periodically
    setInterval(function() {
        fetchInfoHashDetails(infoHash);
    }, 5000);
});

// Fetch info hash details
function fetchInfoHashDetails(infoHash) {
    fetch(`/api/infohash?hash=${infoHash}`)
        .then(response => response.json())
        .then(data => {
            updateInfoHashDetails(data);
            updateFilesTable(data.files || []);
            updatePeersTable(data.peers || []);
            document.getElementById('peers-loading').style.display = 'none';
        })
        .catch(error => {
            console.error('Error fetching info hash details:', error);
            document.getElementById('peers-loading').style.display = 'none';
        });
}

// Update info hash details
function updateInfoHashDetails(data) {
    // Update name
    if (data.name) {
        document.getElementById('info-hash-name').textContent = data.name;
    } else {
        document.getElementById('info-hash-name').textContent = 'Unknown';
    }
    
    // Update size
    if (data.size) {
        const sizeInMB = (data.size / (1024 * 1024)).toFixed(2);
        document.getElementById('info-hash-size').textContent = `${sizeInMB} MB`;
    } else {
        document.getElementById('info-hash-size').textContent = 'Unknown';
    }
    
    // Update peer count
    if (data.peerCount !== undefined) {
        document.getElementById('peer-count').textContent = data.peerCount;
    } else {
        document.getElementById('peer-count').textContent = '0';
    }
    
    // Update metadata status
    if (data.metadataStatus) {
        const statusElement = document.getElementById('metadata-status');
        statusElement.textContent = data.metadataStatus;
        
        // Add appropriate class based on status
        statusElement.className = '';
        if (data.metadataStatus === 'complete') {
            statusElement.classList.add('status-complete');
        } else if (data.metadataStatus === 'in-progress') {
            statusElement.classList.add('status-in-progress');
        } else if (data.metadataStatus === 'queued') {
            statusElement.classList.add('status-queued');
        }
    } else {
        document.getElementById('metadata-status').textContent = 'Not Available';
    }
}

// Update files table
function updateFilesTable(files) {
    const tbody = document.querySelector('#files-table tbody');
    tbody.innerHTML = '';
    
    if (files.length === 0) {
        document.getElementById('no-files-message').style.display = 'block';
        document.getElementById('files-table').style.display = 'none';
        return;
    }
    
    document.getElementById('no-files-message').style.display = 'none';
    document.getElementById('files-table').style.display = 'table';
    
    for (const file of files) {
        const tr = document.createElement('tr');
        
        // Path
        const tdPath = document.createElement('td');
        tdPath.textContent = file.path;
        tr.appendChild(tdPath);
        
        // Size
        const tdSize = document.createElement('td');
        const sizeInMB = (file.size / (1024 * 1024)).toFixed(2);
        tdSize.textContent = `${sizeInMB} MB`;
        tr.appendChild(tdSize);
        
        tbody.appendChild(tr);
    }
}

// Update peers table
function updatePeersTable(peers) {
    const tbody = document.querySelector('#peers-table tbody');
    tbody.innerHTML = '';
    
    if (peers.length === 0) {
        const tr = document.createElement('tr');
        tr.innerHTML = '<td colspan="3" class="no-data">No peers found for this info hash</td>';
        tbody.appendChild(tr);
        return;
    }
    
    for (const peer of peers) {
        const tr = document.createElement('tr');
        
        // IP Address
        const tdIp = document.createElement('td');
        tdIp.textContent = peer.ip;
        tr.appendChild(tdIp);
        
        // Port
        const tdPort = document.createElement('td');
        tdPort.textContent = peer.port;
        tr.appendChild(tdPort);
        
        // Status (placeholder)
        const tdStatus = document.createElement('td');
        tdStatus.innerHTML = '<span class="status-active"></span> Active';
        tr.appendChild(tdStatus);
        
        tbody.appendChild(tr);
    }
}

// Function to acquire metadata for an InfoHash
function acquireMetadata(infoHash) {
    const button = document.getElementById('acquire-metadata-btn');
    button.disabled = true;
    button.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Acquiring...';
    
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
            // Update the status immediately
            document.getElementById('metadata-status').textContent = 'queued';
            document.getElementById('metadata-status').className = 'status-queued';
            
            // Refresh the data after a short delay
            setTimeout(() => {
                fetchInfoHashDetails(infoHash);
                button.disabled = false;
                button.innerHTML = '<i class="fas fa-download"></i> Acquire Metadata';
            }, 1000);
        } else {
            console.error('Failed to start metadata acquisition:', data.error);
            alert('Failed to start metadata acquisition: ' + data.error);
            button.disabled = false;
            button.innerHTML = '<i class="fas fa-download"></i> Acquire Metadata';
        }
    })
    .catch(error => {
        console.error('Error starting metadata acquisition:', error);
        alert('Error starting metadata acquisition: ' + error);
        button.disabled = false;
        button.innerHTML = '<i class="fas fa-download"></i> Acquire Metadata';
    });
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
            
            // Update status indicator
            document.getElementById('status-indicator').classList.remove('status-inactive');
        })
        .catch(error => {
            console.error('Error fetching uptime:', error);
            document.getElementById('status-indicator').classList.add('status-inactive');
        });
}
