import pygame
import time
import serial
import serial.tools.list_ports
import tkinter as tk
from tkinter import ttk, simpledialog

def init_joystick():
    """
    Initializes pygame and the first joystick. Returns the joystick object.
    """
    pygame.init()
    pygame.joystick.init()
    if pygame.joystick.get_count() == 0:
        raise RuntimeError('No joystick detected')
    joystick = pygame.joystick.Joystick(0)
    joystick.init()
    return joystick

def read_joystick(joystick):
    """
    Reads the given joystick's axes and buttons states.
    Args:
        joystick: pygame.joystick.Joystick object
    Returns:
        dict: { 'axes': [float], 'buttons': [bool] }
    """
    pygame.event.pump()  # Process event queue
    axes = [joystick.get_axis(i) for i in range(joystick.get_numaxes())]
    buttons = [joystick.get_button(i) for i in range(joystick.get_numbuttons())]
    return {'axes': axes, 'buttons': buttons}

def show_graphic(state):
    """
    Displays a simple graphic representation of joystick axes and buttons using pygame window.
    Args:
        state (dict): { 'axes': [float], 'buttons': [bool] }
    """
    # Window size and colors
    width, height = 400, 400
    axis_color = (0, 128, 255)  # Left stick
    axis2_color = (255, 128, 0) # Right stick
    button_color = (0, 200, 0)
    button_off_color = (100, 100, 100)
    bg_color = (30, 30, 30)
    
    # Create window if not already created
    if not hasattr(show_graphic, 'screen'):
        show_graphic.screen = pygame.display.set_mode((width, height))
        pygame.display.set_caption('Joystick Visualization')
    screen = show_graphic.screen
    screen.fill(bg_color)

    # Draw left stick (axes 0,1) on left half
    l_cx, l_cy = width // 4, height // 2
    if len(state['axes']) >= 2:
        lx = int(l_cx + state['axes'][0] * (width//6))
        ly = int(l_cy + state['axes'][1] * (height//3))
        pygame.draw.circle(screen, axis_color, (lx, ly), 20)
        pygame.draw.line(screen, (200,200,200), (l_cx, l_cy), (lx, ly), 2)
    # Draw right stick (axes 2,3) on right half
    r_cx, r_cy = 3 * width // 4, height // 2
    if len(state['axes']) >= 4:
        rx = int(r_cx + state['axes'][2] * (width//6))
        ry = int(r_cy + state['axes'][3] * (height//3))
        pygame.draw.circle(screen, axis2_color, (rx, ry), 20)
        pygame.draw.line(screen, (255,180,80), (r_cx, r_cy), (rx, ry), 2)
    # Draw buttons as rectangles at the bottom
    for i, pressed in enumerate(state['buttons']):
        bx = 40 + i*40
        by = height - 40
        color = button_color if pressed else button_off_color
        pygame.draw.rect(screen, color, (bx, by, 30, 30))
    pygame.display.flip()

def send_to_arduino(state, ser):
    """
    Sends joystick state to Arduino via serial.
    Args:
        state (dict): { 'axes': [float], 'buttons': [bool] }
        ser (serial.Serial): Open pyserial Serial object
    """
    # Format: axes and buttons as comma-separated, e.g. "0.12,-0.98,0.00,1,0,0\n"
    axes_str = ','.join(f"{v:.2f}" for v in state['axes'])
    buttons_str = ','.join(str(int(b)) for b in state['buttons'])
    msg = axes_str + ',' + buttons_str + '\n'
    # msg = f"{state['axes'][0]:.2f}\n" if state['axes'] else "0.00\n"
    # print(msg.encode('utf-8'))
    ser.write(msg.encode('utf-8'))

def read_serial_line(ser):
    """
    Reads a line from the serial port, strips the prefix b' and newline,
    and converts comma-separated values to a list of floats.
    
    Args:
        ser: An open pyserial.Serial instance
    
    Returns:
        List of floats parsed from the serial line.
    """
    line = ser.readline().decode('utf-8').strip()
    
    # Remove Python-style byte prefix/suffix if present (e.g. b'...')
    if line.startswith("b'") and line.endswith("'"):
        line = line[2:-1]
    
    try:
        return [float(value) for value in line.split(',')]
    except ValueError as e:
        print("Error parsing line:", line)
        raise e

def select_serial_port():
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        raise RuntimeError("No serial ports found")
    port_names = [port.device for port in ports]

    selected = {'port': None}

    def on_ok():
        selected['port'] = combo.get()
        root.destroy()

    root = tk.Tk()
    root.title("Select Serial Port")
    tk.Label(root, text="Select serial port:").pack(padx=10, pady=10)
    combo = ttk.Combobox(root, values=port_names, state="readonly")
    combo.pack(padx=10, pady=5)
    combo.current(0)
    ok_btn = tk.Button(root, text="OK", command=on_ok)
    ok_btn.pack(pady=10)
    root.mainloop()

    if selected['port'] not in port_names:
        raise RuntimeError("Invalid or no port selected")
    return selected['port']

def main():
    joystick = init_joystick()
    print('Joystick initialized. Reading at 30Hz. Press Ctrl+C to exit.')
    # Popup to select serial port
    port = select_serial_port()
    ser = serial.Serial(port, 115200, timeout=1)
    print(f"Using serial port: {port}")
    try:
        while True:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    raise KeyboardInterrupt
            state = read_joystick(joystick)
            # print(f"Axes: {state['axes']}, Buttons: {state['buttons']}", end='\n', flush=True)
            show_graphic(state)

            try:
                values = read_serial_line(ser)
                print("Parsed:", values)
            except Exception as e:
                print("Skipping invalid line:", e)

            send_to_arduino(state, ser)

            time.sleep(1/50)
    except KeyboardInterrupt:
        print('\nExiting...')
    finally:
        if 'ser' in locals():
            ser.close()
        pygame.quit()

if __name__ == '__main__':
    main()
