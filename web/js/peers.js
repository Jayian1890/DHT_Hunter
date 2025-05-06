// Peers page JavaScript

// Initialize the page
document.addEventListener('DOMContentLoaded', function() {
    // Get the info hash from the URL
    const urlParams = new URLSearchParams(window.location.search);
    const infoHash = urlParams.get('infoHash');

    if (!infoHash) {
        // No info hash provided, show message instead of redirecting
        document.getElementById('info-hash').textContent = 'No Info Hash Selected';
        document.getElementById('info-hash-name').textContent = '';
        document.getElementById('info-hash-size').textContent = '';
        document.getElementById('peer-count').textContent = '';

        // Show the no hash selected message
        document.getElementById('no-hash-selected').style.display = 'block';

        // Hide loading spinner
        document.getElementById('peers-loading').style.display = 'none';

        // Show message in the table
        const tbody = document.querySelector('#peers-table tbody');
        tbody.innerHTML = '<tr><td colspan="5" class="no-data">Please select an info hash from the <a href="index.html">dashboard</a> to view peer details.</td></tr>';

        // Continue with uptime updates
        updateUptime();
        setInterval(updateUptime, 1000);
        return;
    } else {
        // Hide the no hash selected message
        document.getElementById('no-hash-selected').style.display = 'none';
    }

    // Display the info hash
    document.getElementById('info-hash').textContent = infoHash;

    // Fetch info hash details
    fetchInfoHashDetails(infoHash);

    // Fetch peers for the info hash
    fetchPeers(infoHash);

    // Update uptime
    updateUptime();
    setInterval(updateUptime, 1000);
});

// Fetch info hash details
function fetchInfoHashDetails(infoHash) {
    fetch(`/api/infohash?hash=${infoHash}`)
        .then(response => response.json())
        .then(data => {
            // Update info hash details
            if (data.name) {
                document.getElementById('info-hash-name').textContent = `Name: ${data.name}`;
            }

            if (data.size) {
                const sizeInMB = (data.size / (1024 * 1024)).toFixed(2);
                document.getElementById('info-hash-size').textContent = `Size: ${sizeInMB} MB`;
            }

            if (data.peerCount !== undefined) {
                document.getElementById('peer-count').textContent = `Peers: ${data.peerCount}`;
            }
        })
        .catch(error => {
            console.error('Error fetching info hash details:', error);
        });
}

// Fetch peers for the info hash
function fetchPeers(infoHash) {
    document.getElementById('peers-loading').style.display = 'flex';

    fetch(`/api/peers?infoHash=${infoHash}`)
        .then(response => response.json())
        .then(data => {
            updatePeersTable(data);
            document.getElementById('peers-loading').style.display = 'none';
        })
        .catch(error => {
            console.error('Error fetching peers:', error);
            document.getElementById('peers-loading').style.display = 'none';
        });
}

// Update peers table
function updatePeersTable(peers) {
    const tbody = document.querySelector('#peers-table tbody');
    tbody.innerHTML = '';

    if (peers.length === 0) {
        const tr = document.createElement('tr');
        tr.innerHTML = '<td colspan="5" class="no-data">No peers found for this info hash</td>';
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

        // Last Seen (placeholder)
        const tdLastSeen = document.createElement('td');
        tdLastSeen.textContent = 'Just now';
        tr.appendChild(tdLastSeen);

        // Actions
        const tdActions = document.createElement('td');
        tdActions.innerHTML = '<button class="action-btn connect-btn" title="Connect to peer"><i class="fas fa-plug"></i></button>';
        tr.appendChild(tdActions);

        tbody.appendChild(tr);
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
