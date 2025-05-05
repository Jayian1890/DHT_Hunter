// Messages page JavaScript

// Global variables
let currentPage = 1;
let messagesPerPage = 20;
let currentFilter = {
    type: 'all',
    direction: 'all'
};

// Initialize the page
document.addEventListener('DOMContentLoaded', function() {
    // Fetch messages
    fetchMessages();
    
    // Set up filter controls
    document.getElementById('message-type-filter').addEventListener('change', function() {
        currentFilter.type = this.value;
        currentPage = 1;
        fetchMessages();
    });
    
    document.getElementById('message-direction-filter').addEventListener('change', function() {
        currentFilter.direction = this.value;
        currentPage = 1;
        fetchMessages();
    });
    
    // Set up pagination
    document.getElementById('prev-page').addEventListener('click', function() {
        if (currentPage > 1) {
            currentPage--;
            fetchMessages();
        }
    });
    
    document.getElementById('next-page').addEventListener('click', function() {
        currentPage++;
        fetchMessages();
    });
    
    // Update uptime
    updateUptime();
    setInterval(updateUptime, 1000);
    
    // Refresh data periodically
    setInterval(fetchMessages, 10000);
});

// Fetch messages
function fetchMessages() {
    document.getElementById('messages-loading').style.display = 'flex';
    
    // Build query parameters
    const params = new URLSearchParams();
    params.append('limit', messagesPerPage);
    params.append('page', currentPage);
    
    if (currentFilter.type !== 'all') {
        params.append('type', currentFilter.type);
    }
    
    if (currentFilter.direction !== 'all') {
        params.append('direction', currentFilter.direction);
    }
    
    fetch(`/api/messages?${params.toString()}`)
        .then(response => response.json())
        .then(data => {
            updateMessagesTable(data);
            document.getElementById('messages-loading').style.display = 'none';
            
            // Update pagination
            document.getElementById('page-info').textContent = `Page ${currentPage}`;
            document.getElementById('prev-page').disabled = currentPage === 1;
            document.getElementById('next-page').disabled = data.length < messagesPerPage;
        })
        .catch(error => {
            console.error('Error fetching messages:', error);
            document.getElementById('messages-loading').style.display = 'none';
        });
}

// Update messages table
function updateMessagesTable(messages) {
    const tbody = document.querySelector('#messages-table tbody');
    tbody.innerHTML = '';
    
    if (messages.length === 0) {
        const tr = document.createElement('tr');
        tr.innerHTML = '<td colspan="4" class="no-data">No messages found</td>';
        tbody.appendChild(tr);
        return;
    }
    
    for (const message of messages) {
        const tr = document.createElement('tr');
        
        // Time
        const tdTime = document.createElement('td');
        const date = new Date(message.timestamp);
        tdTime.textContent = date.toLocaleTimeString();
        tr.appendChild(tdTime);
        
        // Type
        const tdType = document.createElement('td');
        tdType.textContent = message.type;
        // Add class based on message type
        tdType.classList.add(`message-type-${message.type}`);
        tr.appendChild(tdType);
        
        // Direction
        const tdDirection = document.createElement('td');
        if (message.direction === 'sent') {
            tdDirection.innerHTML = '<i class="fas fa-arrow-right sent-message"></i> Sent';
        } else {
            tdDirection.innerHTML = '<i class="fas fa-arrow-left received-message"></i> Received';
        }
        tr.appendChild(tdDirection);
        
        // Details (placeholder)
        const tdDetails = document.createElement('td');
        tdDetails.innerHTML = '<button class="action-btn view-details-btn" title="View message details"><i class="fas fa-info-circle"></i></button>';
        tr.appendChild(tdDetails);
        
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
