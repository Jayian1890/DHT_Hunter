// Info Hashes page JavaScript

// Global variables
let currentPage = 1;
let infoHashesPerPage = 20;
let currentSort = 'recent';
let searchQuery = '';

// Initialize the page
document.addEventListener('DOMContentLoaded', function() {
    // Fetch info hashes
    fetchInfoHashes();
    
    // Set up search input
    document.getElementById('search-input').addEventListener('input', function() {
        searchQuery = this.value.trim();
        currentPage = 1;
        fetchInfoHashes();
    });
    
    // Set up sort select
    document.getElementById('sort-by').addEventListener('change', function() {
        currentSort = this.value;
        currentPage = 1;
        fetchInfoHashes();
    });
    
    // Set up pagination
    document.getElementById('prev-page').addEventListener('click', function() {
        if (currentPage > 1) {
            currentPage--;
            fetchInfoHashes();
        }
    });
    
    document.getElementById('next-page').addEventListener('click', function() {
        currentPage++;
        fetchInfoHashes();
    });
    
    // Update uptime
    updateUptime();
    setInterval(updateUptime, 1000);
    
    // Refresh data periodically
    setInterval(fetchInfoHashes, 10000);
});

// Fetch info hashes
function fetchInfoHashes() {
    document.getElementById('info-hashes-loading').style.display = 'flex';
    
    // Build query parameters
    const params = new URLSearchParams();
    params.append('limit', infoHashesPerPage);
    params.append('page', currentPage);
    params.append('sort', currentSort);
    
    if (searchQuery) {
        params.append('search', searchQuery);
    }
    
    fetch(`/api/infohashes?${params.toString()}`)
        .then(response => response.json())
        .then(data => {
            updateInfoHashesTable(data);
            document.getElementById('info-hashes-loading').style.display = 'none';
            
            // Update pagination
            document.getElementById('page-info').textContent = `Page ${currentPage}`;
            document.getElementById('prev-page').disabled = currentPage === 1;
            document.getElementById('next-page').disabled = data.length < infoHashesPerPage;
        })
        .catch(error => {
            console.error('Error fetching info hashes:', error);
            document.getElementById('info-hashes-loading').style.display = 'none';
        });
}

// Update info hashes table
function updateInfoHashesTable(infoHashes) {
    const tbody = document.querySelector('#info-hashes-table tbody');
    tbody.innerHTML = '';
    
    if (infoHashes.length === 0) {
        const tr = document.createElement('tr');
        tr.innerHTML = '<td colspan="7" class="no-data">No info hashes found</td>';
        tbody.appendChild(tr);
        return;
    }
    
    for (const infoHash of infoHashes) {
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
        
        // Size
        const sizeTd = document.createElement('td');
        if (infoHash.size) {
            const sizeInMB = (infoHash.size / (1024 * 1024)).toFixed(2);
            sizeTd.textContent = `${sizeInMB} MB`;
        } else {
            sizeTd.textContent = 'Unknown';
        }
        tr.appendChild(sizeTd);
        
        // Peers
        const peersTd = document.createElement('td');
        const peersLink = document.createElement('a');
        peersLink.href = `peers.html?infoHash=${infoHash.hash}`;
        peersLink.textContent = infoHash.peers || 0;
        peersLink.title = 'View peers for this info hash';
        peersLink.className = 'info-hash-link';
        peersTd.appendChild(peersLink);
        tr.appendChild(peersTd);
        
        // First Seen
        const firstSeenTd = document.createElement('td');
        if (infoHash.firstSeen) {
            firstSeenTd.textContent = formatTimeAgo(infoHash.firstSeen);
            firstSeenTd.title = new Date(infoHash.firstSeen).toLocaleString();
        } else {
            firstSeenTd.textContent = 'Unknown';
        }
        tr.appendChild(firstSeenTd);
        
        // Metadata Status
        const statusTd = document.createElement('td');
        if (infoHash.metadataStatus) {
            statusTd.textContent = infoHash.metadataStatus;
            statusTd.className = `status-${infoHash.metadataStatus.toLowerCase().replace(/[^a-z0-9]/g, '-')}`;
        } else {
            statusTd.textContent = 'Not Available';
        }
        tr.appendChild(statusTd);
        
        // Actions
        const actionsTd = document.createElement('td');
        const acquireBtn = document.createElement('button');
        acquireBtn.className = 'action-btn';
        acquireBtn.title = 'Acquire metadata';
        acquireBtn.innerHTML = '<i class="fas fa-download"></i>';
        acquireBtn.addEventListener('click', function() {
            acquireMetadata(infoHash.hash);
        });
        actionsTd.appendChild(acquireBtn);
        tr.appendChild(actionsTd);
        
        tbody.appendChild(tr);
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
            fetchInfoHashes();
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

// Helper function to format info hash
function formatInfoHash(hash) {
    if (hash.length > 16) {
        return hash.substring(0, 8) + '...' + hash.substring(hash.length - 8);
    }
    return hash;
}

// Helper function to format time ago
function formatTimeAgo(timestamp) {
    const now = Date.now();
    const diff = now - timestamp;
    
    const seconds = Math.floor(diff / 1000);
    const minutes = Math.floor(seconds / 60);
    const hours = Math.floor(minutes / 60);
    const days = Math.floor(hours / 24);
    
    if (days > 0) {
        return `${days}d ago`;
    } else if (hours > 0) {
        return `${hours}h ago`;
    } else if (minutes > 0) {
        return `${minutes}m ago`;
    } else {
        return 'Just now';
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
            
            // Update status indicator
            document.getElementById('status-indicator').classList.remove('status-inactive');
        })
        .catch(error => {
            console.error('Error fetching uptime:', error);
            document.getElementById('status-indicator').classList.add('status-inactive');
        });
}
