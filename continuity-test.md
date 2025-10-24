# Continuity Test Procedure

Multimeter continuity test to confirm whether button 4 is physically
connected. Here's how to test

## 1. Test the button switch itself

- Set multimeter to continuity/beep mode
- Place probes on the two button terminals (usually on opposite sides)
- Press button 4 - should beep/show continuity when pressed
- Release - should show open circuit
- This confirms if the button switch itself is functional

## 2. Test button-to-GPIO connection

- With Pi powered OFF, identify the GPIO header pins
  - Find GPIO 23 (pin 16), GPIO 22 (pin 15), GPIO 27 (pin 13) on the 40-pin header
  - Test known working buttons first for reference
- Place one probe on a button terminal
- Touch other probe to each GPIO pin
- Press the button - should show continuity when a connection exists

## 3. Test button-to-ground connection

- Active-low buttons connect to ground when pressed
- Place one probe on button terminal
- Place other probe on any ground pin (pins 6, 9, 14, 20, 25, 30, 34, 39)
- Press button - should show continuity if properly wired

## Expected findings for button 4 consistent with software test

- ✓ Button switch works (shows continuity when pressed)
- ✗ No connection to any GPIO pin
- ✗ No connection to ground

This would confirm the clone manufacturer populated the button but didn't
connect the traces on the PCB to save costs.

