const chartConfig = {temperature: ['temperature-chart', '#126b69'], probe_temperature: ['probe-temperature-chart', '#b34d3d'], humidity: ['humidity-chart', '#e46d3d'], pressure: ['pressure-chart', '#425d88'], turbidity_adc: ['turbidity-chart', '#9b542d'], mpu6050_accel_x: ['accel-x-chart', '#6f4b8b'], mpu6050_gyro_z: ['gyro-z-chart', '#287d7d']};
let previousTurbidity = null;

function value(id, number, suffix = '', missing = '--') { document.getElementById(id).textContent = number === null || number === undefined ? missing : `${number}${suffix}`; }
function drawChart(id, color, values) {
  const canvas = document.getElementById(id); const rect = canvas.getBoundingClientRect(); const scale = window.devicePixelRatio || 1;
  canvas.width = rect.width * scale; canvas.height = rect.height * scale; const ctx = canvas.getContext('2d'); ctx.scale(scale, scale);
  const width = rect.width, height = rect.height, clean = values.filter(v => typeof v === 'number');
  ctx.clearRect(0, 0, width, height); ctx.strokeStyle = '#d9ddd6'; ctx.lineWidth = 1;
  for (let y = 10; y < height; y += 32) { ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke(); }
  if (clean.length < 2) return;
  const min = Math.min(...clean), max = Math.max(...clean), range = max - min || 1; ctx.strokeStyle = color; ctx.lineWidth = 2.5; ctx.beginPath();
  values.forEach((raw, index) => { if (typeof raw !== 'number') return; const x = index / (values.length - 1) * width; const y = height - 10 - (raw - min) / range * (height - 20); index ? ctx.lineTo(x, y) : ctx.moveTo(x, y); }); ctx.stroke();
}
async function refresh() {
  try {
    const [dataResponse, historyResponse] = await Promise.all([fetch('/api/data'), fetch('/api/history')]); const data = await dataResponse.json(); const history = await historyResponse.json();
    const online = data.online === true; document.getElementById('status-dot').classList.toggle('online', online);
    ['system-status', 'esp-status'].forEach(id => { const el = document.getElementById(id); el.textContent = online ? 'ONLINE' : 'OFFLINE'; el.classList.toggle('online', online); });
    document.getElementById('last-update').textContent = online ? `${data.age_seconds}s ago` : 'Awaiting data';
    value('temperature', data.temperature, ' °C', 'DHT22 ERROR'); value('probe-temperature', data.probe_temperature, ' °C', 'DS18B20 ERROR'); value('humidity', data.humidity, ' %', 'DHT22 ERROR'); value('pressure', data.pressure, ' hPa', 'BMP280 ERROR'); value('turbidity', data.turbidity_adc, '', 'TURBIDITY ERROR'); value('turbidity-voltage', data.turbidity_voltage, ' V', 'TURBIDITY ERROR');
    value('gps-status', data.gps_valid ? 'FIXED' : 'NO FIX'); value('gps-coordinates', data.gps_valid ? `${data.gps_latitude}, ${data.gps_longitude}` : null, '', 'Waiting for satellite fix'); value('gps-satellites', data.gps_satellites, '', 'NO DATA');
    value('accel-x', data.mpu6050_accel_x, ' m/s²', 'MPU ERROR'); value('accel-y', data.mpu6050_accel_y, ' m/s²', 'MPU ERROR'); value('accel-z', data.mpu6050_accel_z, ' m/s²', 'MPU ERROR'); value('gyro-x', data.mpu6050_gyro_x, ' rad/s', 'MPU ERROR'); value('gyro-y', data.mpu6050_gyro_y, ' rad/s', 'MPU ERROR'); value('gyro-z', data.mpu6050_gyro_z, ' rad/s', 'MPU ERROR');
    value('distance', data.distance, ' cm'); document.getElementById('distance-unit').textContent = data.distance === null ? 'NO ECHO / optional sensor' : 'cm';
    const changed = previousTurbidity !== null && data.turbidity_adc !== null && Math.abs(data.turbidity_adc - previousTurbidity) > Math.max(100, previousTurbidity * .2); previousTurbidity = data.turbidity_adc;
    document.getElementById('system-mode').textContent = changed ? 'ALERT' : 'NORMAL'; document.getElementById('system-mode').classList.toggle('online', !changed); document.getElementById('turbidity-card').classList.toggle('alerting', changed); document.getElementById('alert').hidden = !changed;
    Object.entries(chartConfig).forEach(([key, config]) => drawChart(config[0], config[1], history.map(item => item[key])));
  } catch (error) { document.getElementById('system-status').textContent = 'OFFLINE'; }
}
refresh(); setInterval(refresh, 1500); window.addEventListener('resize', refresh);