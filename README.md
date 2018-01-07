# M-Tris
Tetris-like game for Arduino
Brought to you by Manifest Solutions

*** Hardware and Assembly ***
The game was designed to run on following hardware:
- Arduino Uno v3
- Adafruit 2.8" TFT touch shield for Arduino with resistive touch screen (product #1651)
- Prototyping shield for Arduino Uno
- SparkFun WiiChuck adapter
- Wii nunchuk controller
- Micro SD card

The shields stack together with the TFT shield on top, the prototyping shield in the middle, and the Uno on the bottom.

The Wii nunchuk connects to the WiiChuck adapter on the prototyping shield.
It should be connected with the clear plastic portion of the nunchuk connector on top (closer to the TFT shield).
The nunchuk must be connected before powering on or resetting the Arduino.

The SD card should be inserted in the SD slot on the TFT shield.
The high score file (MTRISHS.TXT) and the two BMP images belong on the SD card.

*** Operation ***
1. Connect the nunchuk to the WiiChuck adapter, with the clear plastic side of the connector facing up.
2. Connect power to the Arduino.

Controls: Between games
- Z button: Hold down on the high score screen, to start a new game

Controls: During the game
- Joystick left/right: move piece left/right
- Joystick down: move piece rapidly to the bottom
- Z button: rotate piece
- C button: pause or resume the game

Controls: Entering initials
- Joystick up/down/left/right: select letter or number
- Z button: add letter to initials
- C button: remove last letter from initials

*** Scoring ***
Points are received depending on how many horizontal lines are completed by a single piece when it reaches the bottom.
1 row = 10 points
2 rows = 30 points
3 rows = 70 points
4 rows = 150 points

*** FAQ ***
Q. Why is the code so ugly?  Violates separation of concerns, DRY, etc.
A. The Uno has 32KB of program memory, and 2KB of data shared between the heap and the stack.  Getting all of the necessary libraries to load, and still having room for program logic, proved to be a challenge.  I tried to keep it very readable, but I had to make compromises.

Q. Any lessons learned to share?
A. 1. The F() macro helped a lot to free up memory, where the program was using literal strings.  Normally, literal strings are loaded into program memory and static data.  With the F() macro, they are only loaded from Flash to RAM when actually used.
2. Also, when working with the SD card on the TFT, I had to call the SD.begin() method each time the high score file needed to be read or written.  I'm not sure why; maybe communication with the display interferes with the SD card library, since both are on the TFT.
3. The bmpDraw() method requires the use of the Serial library, and it requires that the caller invoke Serial.begin().
