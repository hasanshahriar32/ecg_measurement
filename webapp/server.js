const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const mqtt = require('mqtt');
const cors = require('cors');
const path = require('path');

const app = express();
const server = http.createServer(app);
const io = socketIo(server, {
    cors: {
        origin: "*",
        methods: ["GET", "POST"]
    }
});

// Middleware
app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

// MQTT Configuration for HiveMQ Cloud (matching ESP32 settings)
const MQTT_BROKER = 'd5e9ca698a2a4640b81af8b8e3e6e1e4.s1.eu.hivemq.cloud';
const MQTT_PORT = 8883;
const MQTT_TOPIC = 'mrhasan/heart'; // Topic where your ESP32 publishes data
const MQTT_USERNAME = 'Paradox';
const MQTT_PASSWORD = 'Paradox1';

// Connect to MQTT broker with SSL/TLS
const mqttClient = mqtt.connect(`mqtts://${MQTT_BROKER}:${MQTT_PORT}`, {
    clientId: `ecg_webapp_${Math.random().toString(16).substr(2, 8)}`,
    username: MQTT_USERNAME,
    password: MQTT_PASSWORD,
    clean: true,
    connectTimeout: 4000,
    reconnectPeriod: 1000,
    rejectUnauthorized: false, // For testing only - use proper certificates in production
});

// MQTT event handlers
mqttClient.on('connect', () => {
    console.log('Connected to HiveMQ Cloud broker');
    
    // Subscribe to ECG data topic
    mqttClient.subscribe(MQTT_TOPIC, (err) => {
        if (err) {
            console.error('Failed to subscribe to topic:', err);
        } else {
            console.log(`Subscribed to topic: ${MQTT_TOPIC}`);
        }
    });
});

mqttClient.on('message', (topic, message) => {
    try {
        const data = JSON.parse(message.toString());
        console.log('Received ECG data:', data);
        
        // Validate data structure
        if (data.userId && data.dataType === 'ecg_analysis') {
            // Emit to all connected clients
            io.emit('ecg_data', {
                ...data,
                receivedAt: new Date().toISOString()
            });
        }
    } catch (error) {
        console.error('Error parsing MQTT message:', error);
    }
});

mqttClient.on('error', (error) => {
    console.error('MQTT connection error:', error);
});

mqttClient.on('offline', () => {
    console.log('MQTT client offline');
});

mqttClient.on('reconnect', () => {
    console.log('MQTT client reconnecting...');
});

// Socket.IO connection handling
io.on('connection', (socket) => {
    console.log('New client connected:', socket.id);
    
    // Send connection confirmation
    socket.emit('connection_status', { 
        status: 'connected',
        message: 'Connected to ECG monitoring system'
    });
    
    socket.on('disconnect', () => {
        console.log('Client disconnected:', socket.id);
    });
    
    // Handle client requesting to subscribe to specific device
    socket.on('subscribe_device', (deviceId) => {
        console.log(`Client ${socket.id} subscribed to device: ${deviceId}`);
        socket.join(deviceId);
    });
});

// Routes
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

app.get('/api/status', (req, res) => {
    res.json({
        status: 'running',
        mqtt_connected: mqttClient.connected,
        clients_connected: io.sockets.sockets.size,
        timestamp: new Date().toISOString()
    });
});

// Start server
const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
    console.log(`ECG Monitor Web App running on port ${PORT}`);
    console.log(`Open http://localhost:${PORT} in your browser`);
});

// Graceful shutdown
process.on('SIGINT', () => {
    console.log('Shutting down gracefully...');
    mqttClient.end();
    server.close(() => {
        console.log('Server closed');
        process.exit(0);
    });
});
