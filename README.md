# ptzcontrol
Arduino code to allow ATEM control of an analogue pan/tilt head

Scope

Blackmagic ATEM products can incorporate PTZ control commands within the SDI data stream. This can be used to control some PTZ cameras, but the supported options are limited and can be costly.
This project is intended to use an inexpensive, generic pan & tilt head to provide PT functionality to any small camera, under ATEM control.

Requirements

The following hardware is needed:
- Hague MPH Pan & Tilt Power Head 360° (£160) - this is sold under various names & brands in different territories
- Arduino-compatible board with R3 footprint (<£10)
- Blackmagic Design 3G-SDI Arduino Shield (£90)
- Optional: Arduino Shield board, for ease of wiring
- Resistors 4x 5KΩ approx
- 12V DC power supply

The control software needed to run on the Arduino is supplied on an Open Source basis and can freely be modified and upgraded.

Hardware Overview

The goal of the project is to use the Arduino as an intermediary between a Blackmagic ATEM vision mixer and the PT head, to convert the control signals embedded in the ATEM’s SDI stream to the control signals expected by the PT head.

Input

In order to do this, the Arduino needs to be able to interpret the SDI stream. The easiest way to accomplish this is to use the Blackmagic 3G-SDI Arduino Shield. This is a small board which piggybacks onto the Arduino R3-type controller, combined with an API which extends the Arduino’s programming language to read and write SDI metadata.
It’s trivial to add a separate joystick controller, allowing manual control of the PT head, for example if the ATEM signal is not available or not functioning as expected. This project makes use of a generic 2-axis analogue joystick with pushbutton function for this purpose.

Output

The Hague PT head is controlled by default using a dedicated controller which connects through a 7-pin DIN cable. This provides the head with an analogue voltage to control the head’s speed (for both axes) plus a +5V (nominal) digital signal for up/down/left/right control.
The head is supplied with an extension DIN cable which can be adapted to connect the Arduino controller to the head without the need to modify the supplied controller.
By default, the control PCB in the Hague PT head takes a single analogue signal from the controller to control the head speed on both axes. The head can be modified to allow separate speed controls for each axis. This also involves removing the unused +5V supply from the DIN cable to use its wire as a data signal for this speed control. Therefore, once this modification is made, the head can no longer be used with its own controller.
For the physical connections between the Arduino and the DIN cable, it’s easiest to make use of a blank Arduino shield circuit board, which avoids the need to solder components and connectors directly onto the Arduino.

Connections

PTZ cable

The DIN extension cable supplied with my PT head was colour coded as follows. Others may vary. The most important part is to identify the ground, +5V and speed control voltage pins, as the others are straightforward to remap in software.
Yellow: left
Green: right
Black: up
Blue: down
Red: ground
Purple: speed control voltage
White: +5V
On the Arduino we are going to avoid using pins 0 and 1, as they may be used elsewhere for serial connections. It is easiest to solder the wires from the DIN cable to a blank Arduino Shield and then connect jumper wires to the Arduino pins.
Connect ground to a grounded Arduino pin.
Do not connect +5V; power is supplied by the Arduino so this is not needed. It can be soldered to an unused area of the blank Shield.
Connect direction pins as follows: left to pin 3, right 4, up 5, down 6. (These can be changed in the code if required.) Each pin should be connected via a 5KΩ resistor; this may not be necessary but helps ensure the Arduino is not overloaded.
Connect the speed control voltage to pin 10.

Joystick

Connect as follows:
Switch to pin 2 (it must be connected to an interrupt-compatible pin)
VRy to analogue pin 1
VRx to analogue pin 0
+5V to +5V
Ground to ground

Power

The BM Shield for Arduino requires a 12V power supply. This uses a 5.5mm / 2.5mm barrel connector. Power only the BM Shield, not the Arduino board, as although the Shield can provide power to the Arduino board, the Arduino board will not supply sufficient power to the Shield.

Software

Blackmagic provided me with a basic script which polls the SDI input for control commands and parses them. Documentation is available on the Blackmagic website. I added custom code to interpret the control commands using category 11.0 (PT control), to allow joystick control, and to turn both of these inputs into an output compatible with the PT head.
The code is certainly open to improvement.

Principles

The button-press function on the joystick is used to trigger an interrupt which switches between joystick control mode and ATEM control mode.
The output from the Arduino drives 5 functions of the PT head: up, down, left, right and speed. This means only a single speed control is available for both axes, by default.
There are two ways to overcome this limitation:
- The code will use the input y-axis speed to control the PT speed, unless the y-axis is not being used, in which case it’ll use the x-axis input to control the PT speed. A side effect of this is that diagonal movement will only ever be done at 45 degrees.
- It would be possible to remove the +5V supply on the DIN cable and repurpose this wire to directly supply an analogue speed controller voltage to each individual motor control chip within the PT head. The code has been written to use this if available (on pin 11 by default). Note that if the PT head control board is modified, it will no longer be possible to use any other controller for the head.
The code should never trigger both up and down, or left and right, functions at the same time; it is written so each output pin is disabled BEFORE its complement is enabled.
The code outputs troubleshooting data to the USB serial port; if the USB port is not connected this will be ignored.

Installation

Blackmagic’s code makes use of the Streaming.h library, so this should be installed. The Blackmagic libraries BMDSIDControl.h and BMSDIControlShieldRegisters.h should also be installed.

Setup

The code is ready to run but output pin allocations can be customised in code if required, as can joystick pin allocations.
