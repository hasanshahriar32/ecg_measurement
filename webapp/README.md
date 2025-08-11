# ECG Monitor Web Application

A real-time ECG monitoring web application that connects to HiveMQ MQTT broker and displays ECG data through a web interface using Socket.IO.

## Features

- Real-time ECG data visualization
- MQTT connection to HiveMQ broker
- WebSocket communication with clients
- Responsive web dashboard
- Heart rate trend charts
- Heart rate variability (HRV) monitoring
- Device status monitoring
- Data logging

## Installation

1. Navigate to the webapp directory:
```bash
cd webapp
```

2. Install dependencies:
```bash
npm install
```

3. Start the application:
```bash
npm start
```

Or for development with auto-restart:
```bash
npm run dev
```

4. Open your browser and go to:
```
http://localhost:3000
```

## Configuration

### MQTT Settings

The application is configured to connect to HiveMQ Cloud broker:
- **Broker**: `d5e9ca698a2a4640b81af8b8e3e6e1e4.s1.eu.hivemq.cloud`
- **Port**: `8883` (SSL/TLS)
- **Topic**: `mrhasan/heart`
- **Username**: `Paradox`
- **Password**: `Paradox1`

To change these settings, modify the constants in `server.js`:

```javascript
const MQTT_BROKER = 'your-broker-url.com';
const MQTT_PORT = 1883;
const MQTT_TOPIC = 'your/topic';
```

### ESP32 MQTT Topic

Make sure your ESP32 device publishes to the same topic (`mrhasan/heart`) that the web app subscribes to.

## Data Format

The application expects ECG data in the following JSON format:

```json
{
  "userId": "BW8NUP21AWMkI0xrrI2nxBP6Xd92",
  "dataType": "ecg_analysis",
  "hp": 0,
  "threshold": 8,
  "bpm": 75,
  "baselineHR": 70,
  "rmssd": 45.5,
  "hrTrend": 5,
  "timestamp": "1546681",
  "deviceId": "ESP32_4B00"
}
```

## Dashboard Features

### Real-time Monitoring
- **Heart Rate (BPM)**: Current beats per minute
- **Heart Points (HP)**: Custom heart scoring metric
- **RMSSD**: Root mean square of successive differences (HRV metric)
- **Threshold**: Current detection threshold

### Visualizations
- **Heart Rate Trend Chart**: Real-time heart rate over time
- **HRV Chart**: Heart rate variability measurements
- **Data Log**: Timestamped data entries

### Connection Status
- Visual indicator showing MQTT and WebSocket connection status
- Device information display
- Last update timestamp

## API Endpoints

- `GET /`: Main dashboard page
- `GET /api/status`: Application status and connection info

## Dependencies

- **express**: Web server framework
- **socket.io**: Real-time communication
- **mqtt**: MQTT client for broker connection
- **cors**: Cross-origin resource sharing

## Troubleshooting

1. **MQTT Connection Issues**:
   - Check if the broker URL is correct
   - Verify network connectivity
   - Ensure the topic matches your ESP32 publishing topic

2. **No Data Appearing**:
   - Verify ESP32 is publishing to the correct topic
   - Check MQTT broker connectivity
   - Ensure data format matches expected JSON structure

3. **WebSocket Issues**:
   - Check browser console for errors
   - Verify port 3000 is not blocked
   - Try refreshing the page

## License

MIT
