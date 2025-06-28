// ===== GLOBAL VARIABLES =====
let port, writer, reader;
let pollInterval = null;

// Joystick visualization elements
const leftStickViz = document.querySelector('.stick-left .stick-position');
const rightStickViz = document.querySelector('.stick-right .stick-position');
const leftStickXDisplay = document.getElementById('leftStickX');
const leftStickYDisplay = document.getElementById('leftStickY');
const rightStickXDisplay = document.getElementById('rightStickX');
const rightStickYDisplay = document.getElementById('rightStickY');
const buttonDisplay = document.getElementById('buttonDisplay');

// ===== SERIAL COMMUNICATION =====
async function connectSerial() {
  try {
    port = await navigator.serial.requestPort();
    await port.open({ baudRate: 115200 });

    writer = port.writable.getWriter();
    reader = port.readable.getReader();
    
    console.log("Serial connected successfully");
    readSerial();
  } catch (err) {
    console.error("Serial connection failed:", err);
    alert("Serial connection failed: " + err.message);
  }
}

// ===== JOYSTICK VISUALIZATION =====
function updateJoystickDisplay(gamepad) {
  if (!gamepad) {
    // No gamepad connected, reset display
    leftStickXDisplay.textContent = '0.00';
    leftStickYDisplay.textContent = '0.00';
    rightStickXDisplay.textContent = '0.00';
    rightStickYDisplay.textContent = '0.00';
    leftStickViz.style.transform = 'translate(0px, 0px)';
    rightStickViz.style.transform = 'translate(0px, 0px)';
    buttonDisplay.innerHTML = '<p>No gamepad connected.</p>';
    return;
  }

  // Update stick values and displays
  const leftStickX = gamepad.axes[0] ? gamepad.axes[0].toFixed(2) : '0.00';
  const leftStickY = gamepad.axes[1] ? gamepad.axes[1].toFixed(2) : '0.00';
  const rightStickX = gamepad.axes[2] ? gamepad.axes[2].toFixed(2) : '0.00';
  const rightStickY = gamepad.axes[3] ? gamepad.axes[3].toFixed(2) : '0.00';

  leftStickXDisplay.textContent = leftStickX;
  leftStickYDisplay.textContent = leftStickY;
  rightStickXDisplay.textContent = rightStickX;
  rightStickYDisplay.textContent = rightStickY;

  // Move the visual dots within their containers
  leftStickViz.style.setProperty('transform', `translate(${leftStickX * 40}px, ${leftStickY * 40}px)`);
  rightStickViz.style.setProperty('transform', `translate(${rightStickX * 40}px, ${rightStickY * 40}px)`);

  // Update button status
  buttonDisplay.innerHTML = '';
  gamepad.buttons.forEach((button, index) => {
    const buttonIndicator = document.createElement('div');
    buttonIndicator.classList.add('button-indicator');
    buttonIndicator.textContent = index;
    if (button.pressed) {
      buttonIndicator.classList.add('active');
    }
    buttonDisplay.appendChild(buttonIndicator);
  });
}

// ===== GAMEPAD POLLING =====
async function pollGamepad() {
  const gamepads = navigator.getGamepads();
  const gamepad = gamepads[0];

  updateJoystickDisplay(gamepad);

  // Send data to serial if connected
  if (writer && gamepad) {
    try {
      const axesStr = gamepad.axes.slice(0, 4).map(v => v.toFixed(2)).join(',');
      const buttonsStr = gamepad.buttons.map(b => b.pressed ? 1 : 0).join(',');
      const msg = `${axesStr},${buttonsStr}\n`;
      console.log(msg);
      const data = new TextEncoder().encode(msg);
      
      await writer.write(data);
    } catch (err) {
      console.error("Failed to write to serial:", err);
    }
  }
}

// ===== SERIAL READING =====
async function readSerial() {
  const decoder = new TextDecoder();
  let buffer = '';
  
  while (port && reader) {
    try {
      const { value, done } = await reader.read();
      if (done) break;
      
      if (value) {
        buffer += decoder.decode(value);
        let idx;
        
        while ((idx = buffer.indexOf('\n')) !== -1) {
          const msg = buffer.slice(0, idx);
          const serialIncomeElement = document.getElementById('serialIncome');
          
          if (serialIncomeElement) {
            serialIncomeElement.textContent = msg;
          }
          
          console.log('Received:', msg);
          buffer = buffer.slice(idx + 1);
        }
      }
    } catch (err) {
      console.error('Serial read error:', err);
      break;
    }
  }
}

// ===== EVENT LISTENERS =====
window.addEventListener("gamepadconnected", (e) => {
  console.log("Gamepad connected at index %d: %s. %d buttons, %d axes.",
    e.gamepad.index, e.gamepad.id,
    e.gamepad.buttons.length, e.gamepad.axes.length);
  
  if (!pollInterval) {
    pollInterval = setInterval(pollGamepad, 50); // 50Hz polling
  }
  pollGamepad(); // Immediate update on connection
});

window.addEventListener("gamepaddisconnected", (e) => {
  console.log("Gamepad disconnected from index %d: %s",
    e.gamepad.index, e.gamepad.id);
  
  if (pollInterval) {
    clearInterval(pollInterval);
    pollInterval = null;
  }
  pollGamepad(); // Clear display after disconnection
});

// ===== UTILITY FUNCTIONS =====
// Placeholder functions for UI elements
function export2csv() {
  console.log("Export to CSV functionality not implemented");
}

function openLimitsModal() {
  console.log("Open limits modal functionality not implemented");
}

function about() {
  console.log("About functionality not implemented");
}

// Initialize display on page load
document.addEventListener('DOMContentLoaded', () => {
  pollGamepad(); // Check for already connected gamepads
});