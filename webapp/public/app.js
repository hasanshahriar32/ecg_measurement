// Socket.IO connection
const socket = io();

// Chart instances
let hrChart, hrvChart;

// Data storage
const hrData = [];
const hrvData = [];
const maxDataPoints = 50;

// DOM elements
const elements = {
    connectionStatus: document.getElementById('connectionStatus'),
    deviceId: document.getElementById('deviceId'),
    userId: document.getElementById('userId'),
    lastUpdate: document.getElementById('lastUpdate'),
    bpm: document.getElementById('bpm'),
    hp: document.getElementById('hp'),
    rmssd: document.getElementById('rmssd'),
    threshold: document.getElementById('threshold'),
    hrTrend: document.getElementById('hrTrend'),
    dataLog: document.getElementById('dataLog')
};

// Initialize charts
function initCharts() {
    const chartConfig = {
        type: 'line',
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                x: {
                    display: true,
                    title: {
                        display: true,
                        text: 'Time'
                    }
                },
                y: {
                    display: true,
                    beginAtZero: false
                }
            },
            plugins: {
                legend: {
                    display: false
                }
            },
            animation: {
                duration: 0
            }
        }
    };

    // Heart Rate Chart
    const hrCtx = document.getElementById('hrChart').getContext('2d');
    hrChart = new Chart(hrCtx, {
        ...chartConfig,
        data: {
            labels: [],
            datasets: [{
                label: 'Heart Rate (BPM)',
                data: [],
                borderColor: '#e74c3c',
                backgroundColor: 'rgba(231, 76, 60, 0.1)',
                tension: 0.4,
                fill: true
            }]
        },
        options: {
            ...chartConfig.options,
            scales: {
                ...chartConfig.options.scales,
                y: {
                    ...chartConfig.options.scales.y,
                    title: {
                        display: true,
                        text: 'BPM'
                    },
                    min: 50,
                    max: 150
                }
            }
        }
    });

    // HRV Chart
    const hrvCtx = document.getElementById('hrvChart').getContext('2d');
    hrvChart = new Chart(hrvCtx, {
        ...chartConfig,
        data: {
            labels: [],
            datasets: [{
                label: 'RMSSD (ms)',
                data: [],
                borderColor: '#3498db',
                backgroundColor: 'rgba(52, 152, 219, 0.1)',
                tension: 0.4,
                fill: true
            }]
        },
        options: {
            ...chartConfig.options,
            scales: {
                ...chartConfig.options.scales,
                y: {
                    ...chartConfig.options.scales.y,
                    title: {
                        display: true,
                        text: 'RMSSD (ms)'
                    },
                    min: 0
                }
            }
        }
    });
}

// Update connection status
function updateConnectionStatus(status, message) {
    elements.connectionStatus.className = `connection-status ${status}`;
    elements.connectionStatus.innerHTML = `
        <i class="fas fa-circle"></i>
        <span>${message}</span>
    `;
}

// Update vital signs display
function updateVitalSigns(data) {
    elements.deviceId.textContent = data.deviceId || '-';
    elements.userId.textContent = data.userId || '-';
    elements.lastUpdate.textContent = new Date().toLocaleTimeString();
    
    elements.bpm.textContent = data.bpm || '--';
    elements.hp.textContent = data.hp || '--';
    elements.rmssd.textContent = data.rmssd ? data.rmssd.toFixed(1) : '--';
    elements.threshold.textContent = data.threshold || '--';
    
    // Update HR trend
    const trendElement = elements.hrTrend;
    const trendValue = data.hrTrend;
    
    if (trendValue !== undefined && trendValue !== null) {
        trendElement.textContent = trendValue > 0 ? `+${trendValue}` : `${trendValue}`;
        trendElement.className = trendValue > 0 ? 'vital-trend positive' : 
                                 trendValue < 0 ? 'vital-trend negative' : 'vital-trend';
    } else {
        trendElement.textContent = '--';
        trendElement.className = 'vital-trend';
    }
}

// Update charts
function updateCharts(data) {
    const currentTime = new Date().toLocaleTimeString();
    
    // Update HR chart
    hrData.push({
        time: currentTime,
        value: data.bpm || 0
    });
    
    // Update HRV chart
    hrvData.push({
        time: currentTime,
        value: data.rmssd || 0
    });
    
    // Keep only recent data points
    if (hrData.length > maxDataPoints) {
        hrData.shift();
    }
    if (hrvData.length > maxDataPoints) {
        hrvData.shift();
    }
    
    // Update HR chart
    hrChart.data.labels = hrData.map(point => point.time);
    hrChart.data.datasets[0].data = hrData.map(point => point.value);
    hrChart.update('none');
    
    // Update HRV chart
    hrvChart.data.labels = hrvData.map(point => point.time);
    hrvChart.data.datasets[0].data = hrvData.map(point => point.value);
    hrvChart.update('none');
}

// Add log entry
function addLogEntry(data) {
    const logContainer = elements.dataLog;
    const logEntry = document.createElement('p');
    logEntry.className = 'log-item new';
    
    const timestamp = new Date().toLocaleTimeString();
    logEntry.innerHTML = `
        <strong>[${timestamp}]</strong> 
        BPM: ${data.bpm}, HP: ${data.hp}, RMSSD: ${data.rmssd}, 
        Trend: ${data.hrTrend}, Device: ${data.deviceId}
    `;
    
    // Add to top of log
    if (logContainer.firstChild) {
        logContainer.insertBefore(logEntry, logContainer.firstChild);
    } else {
        logContainer.appendChild(logEntry);
    }
    
    // Remove old entries (keep last 20)
    const logItems = logContainer.querySelectorAll('.log-item');
    if (logItems.length > 20) {
        logItems[logItems.length - 1].remove();
    }
    
    // Remove animation class after animation completes
    setTimeout(() => {
        logEntry.classList.remove('new');
    }, 2000);
}

// Socket event listeners
socket.on('connect', () => {
    console.log('Connected to server');
    updateConnectionStatus('connected', 'Connected to ECG Monitor');
});

socket.on('disconnect', () => {
    console.log('Disconnected from server');
    updateConnectionStatus('disconnected', 'Disconnected from server');
});

socket.on('connection_status', (data) => {
    console.log('Connection status:', data);
    updateConnectionStatus('connected', data.message);
});

socket.on('ecg_data', (data) => {
    console.log('Received ECG data:', data);
    
    // Update all displays
    updateVitalSigns(data);
    updateCharts(data);
    addLogEntry(data);
});

socket.on('connect_error', (error) => {
    console.error('Connection error:', error);
    updateConnectionStatus('disconnected', 'Connection failed');
});

// Initialize the application
document.addEventListener('DOMContentLoaded', () => {
    console.log('ECG Monitor Dashboard initialized');
    updateConnectionStatus('connecting', 'Connecting to server...');
    initCharts();
    
    // Clear initial log message when first data arrives
    let firstDataReceived = false;
    const originalAddLogEntry = addLogEntry;
    addLogEntry = function(data) {
        if (!firstDataReceived) {
            elements.dataLog.innerHTML = '';
            firstDataReceived = true;
        }
        originalAddLogEntry(data);
    };
});

// Handle page visibility changes
document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'visible') {
        // Reconnect if needed when page becomes visible
        if (!socket.connected) {
            socket.connect();
        }
    }
});

// Handle window resize for charts
window.addEventListener('resize', () => {
    if (hrChart && hrvChart) {
        hrChart.resize();
        hrvChart.resize();
    }
});
