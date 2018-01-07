#include <Wire.h>
#include <wiinunchuck.h>  // Nunchuk library, uses analog pins 4 and 5
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include <SD.h>

// The display also uses hardware SPI, plus #9 & #10
#define TFT_CS 10
#define TFT_DC 9
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

// Use digital pin 4 for the SD card
#define SD_CS 4

// Pixel buffer size for loading images
#define BUFFPIXEL 20

#define JOY_X_LIMIT 50
#define JOY_Y_LIMIT 50
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define LEFT_1UP 20
#define TOP_1UP 0
#define SCORE_LEFT 0
#define SCORE_TOP 20
#define SCORE_WIDTH 72
#define SCORE_HEIGHT 20
#define LEFT_HI 100
#define TOP_HI 0
#define HI_SCORE_LEFT 120
#define HI_SCORE_TOP 20
#define HI_SCORE_WIDTH 72
#define HI_SCORE_HEIGHT 20
#define GRID_WIDTH 10
#define GRID_HEIGHT 12
#define LEFT_EDGE 20
#define TOP_EDGE 75
#define WALL_THICKNESS 5
#define BLOCK_WIDTH 20
#define BLOCK_HEIGHT 20

// global variables (game state)
long boardState[12];  // 12x10 board, each long is 10 columns of 3 bits each representing a color
int activePiece, piece_x, piece_y, loopsPerDrop, loopsRemaining;
boolean isGameOver = true;
boolean isPaused = false;
boolean isInitials = false; // user is entering initials for a high score
boolean isIdle = true; // prevent repeated actions by consecutive nunchuk samples
boolean stopDrop = false; // prevent the next piece from falling after slamming the last one home
long initialsTimeUp = 0;
long score = 0;
long highScore = 0;
long hsScores[10] = {320, 200, 160, 120, 100, 80, 60, 40, 20, 10};
char hsInitials[30] = {'P', 'I', 'G', 'N', 'J', 'M', 'M', 'A', 'N', 'I', 'F', ' ', 'E', 'S', 'T',
                       'M', 'D', 'N', 'A', 'B', 'C', 'F', 'O', 'O', 'B', 'A', 'R', 'B', 'A', 'Z'};
char currInits[3] = {' ', ' ', ' '};
int currInitIdx = 0;
byte selectIdx = 0;
byte secsLeft = 0;

void setup() {
  Serial.begin(9600);
  
  // Initialize SD card
  SD.begin(SD_CS);
  initializeHighScores();

  // Initialize the Wii nunchuk
  nunchuk_setpowerpins();
  nunchuk_init();

  // Initialize the random number generator
  delay(250);
  nunchuk_get_data();
  randomSeed(millis());

  // Initialize the display
  tft.begin();

  if (isInitials) {
    drawInitialsScreen();
  } else if (isGameOver) {
    drawHighScoreScreen();
  } else {
    // Start the game
    drawNewGameScreen();
    initializeBoardState();
    updateScore(0);
    drawActivePiece(true);
  }
}

void loop() {
  delay(20);
  nunchuk_get_data();

  if (isInitials) loopInitials();
  else if (isGameOver) loopGameOver();
  else loopGameInProgress();
}

void loopGameInProgress() {
  if (nunchuk_cbutton()) {
    // C button pressed - pause or resume game
    if (isPaused) {
      isPaused = false;
      drawOnResume();
    } else {
      isPaused = true;
      drawPauseScreen();
    }
  }

  if (isPaused) return;

  // Use nunchuk joystick data and buttons to move/rotate the piece
  // Player must wait one cycle between left/right/rotate moves
  boolean isCurrentCycleIdle = true;
  int jx = (nunchuk_joy_x() - 125) / 2;
  int jy = (125 - nunchuk_joy_y()) / 2;
  if (jx < -30) {
    isCurrentCycleIdle = false;
    // Move piece left
    if (isIdle) moveActivePiece(-1, 0);
  } else if (jx > 30) {
    isCurrentCycleIdle = false;
    // Move piece right
    if (isIdle) moveActivePiece(1, 0);
  }
  if (isGameOver) return;
  if (jy > 30) {
    isCurrentCycleIdle = false;
    // Move piece downward
    if (!stopDrop) moveActivePiece(0, 1);
  }
  if (isGameOver) return;
  if (nunchuk_zbutton()) {
    isCurrentCycleIdle = false;
    // Z button pressed - rotate piece
    if (isIdle) rotateActivePiece();
  }
  if (isCurrentCycleIdle) stopDrop = false;
  isIdle = isCurrentCycleIdle;

  // Check whether to drop the piece
  loopsRemaining--;
  if (loopsRemaining == 0) {
    loopsRemaining = loopsPerDrop;
    if (loopsPerDrop > 40) loopsPerDrop--;
    moveActivePiece(0, 1);
  }
}

void loopGameOver() {
    // Cycle through splash screens
    boolean zPressed = false;
    if (nunchuk_zbutton()) zPressed = true;
    if (!zPressed) bmpDraw("MTrisT~1.bmp", 0, 0);
    if (nunchuk_zbutton()) zPressed = true;
    if (!zPressed) delay(2000);
    if (nunchuk_zbutton()) zPressed = true;
    if (!zPressed) bmpDraw("Manife~1.bmp", 0, 0);
    if (nunchuk_zbutton()) zPressed = true;
    if (!zPressed) delay(2000);
    if (nunchuk_zbutton()) zPressed = true;
    if (!zPressed) drawHighScoreScreen();
    if (nunchuk_zbutton()) zPressed = true;
    if (!zPressed) delay(5000);
    if (nunchuk_zbutton()) zPressed = true;
    
    // If the user pressed the Z button, start a new game
    if (zPressed) {
      isGameOver = false;
      stopDrop = false;
      drawNewGameScreen();
      initializeBoardState();
      updateScore(0);
    }
}

// User is entering their initials
void loopInitials() {
  boolean isNowIdle = true;
  if (nunchuk_zbutton()) {
    isNowIdle = false;
    if (isIdle) {
      // user selected a letter
      if (selectIdx < 26) currInits[currInitIdx] = 'A' + selectIdx;
      else currInits[currInitIdx] = '0' + (selectIdx - 26);
      if (currInitIdx == 0) tft.setCursor(69, 20);
      else if (currInitIdx == 1) tft.setCursor(109, 20);
      else tft.setCursor(149, 20);
      tft.setTextSize(4);
      tft.setTextColor(ILI9341_GREEN);
      tft.print(currInits[currInitIdx++]);
      if (currInitIdx == 3) {
        updateHighScores(currInits);
        isInitials = false;
        currInitIdx = 0;
        currInits[0] = currInits[1] = currInits[2] = ' ';
        delay(2000);  // brief delay to show the user the letter they selected
        drawHighScoreScreen();
        return;
      }
    }
  }

  if (nunchuk_cbutton()) {
    isNowIdle = false;
    // user wants to erase the last letter
    if (isIdle) {
      if (currInitIdx > 0) {
        currInitIdx--;
        currInits[currInitIdx] = ' ';
        if (currInitIdx == 0) tft.fillRect( 66, 17, 26, 33, ILI9341_BLACK);
        if (currInitIdx == 1) tft.fillRect(106, 17, 26, 33, ILI9341_BLACK);
      }
    }
  }

  int jx = (nunchuk_joy_x() - 125) / 2;
  int jy = (125 - nunchuk_joy_y()) / 2;
  if (jx < -30) {
    isNowIdle = false;
    // Move left
    if (isIdle) {
      tft.drawRect(30 + (selectIdx % 6) * 30, 55 + (selectIdx / 6) * 35, 28, 33, ILI9341_BLUE);
      if (selectIdx % 6 == 0) selectIdx += 5; else selectIdx--;
      tft.drawRect(30 + (selectIdx % 6) * 30, 55 + (selectIdx / 6) * 35, 28, 33, ILI9341_YELLOW);
    }
  } else if (jx > 30) {
    isNowIdle = false;
    // Move right
    if (isIdle) {
      tft.drawRect(30 + (selectIdx % 6) * 30, 55 + (selectIdx / 6) * 35, 28, 33, ILI9341_BLUE);
      if (selectIdx % 6 == 5) selectIdx -= 5; else selectIdx++;
      tft.drawRect(30 + (selectIdx % 6) * 30, 55 + (selectIdx / 6) * 35, 28, 33, ILI9341_YELLOW);
    }
  }
  if (jy < -30) {
    isNowIdle = false;
    // Move up
    if (isIdle) {
      tft.drawRect(30 + (selectIdx % 6) * 30, 55 + (selectIdx / 6) * 35, 28, 33, ILI9341_BLUE);
      if (selectIdx / 6 == 0) selectIdx += 30; else selectIdx -= 6;
      tft.drawRect(30 + (selectIdx % 6) * 30, 55 + (selectIdx / 6) * 35, 28, 33, ILI9341_YELLOW);
    }
  }
  if (jy > 30) {
    isNowIdle = false;
    // Move down
    if (isIdle) {
      tft.drawRect(30 + (selectIdx % 6) * 30, 55 + (selectIdx / 6) * 35, 28, 33, ILI9341_BLUE);
      if (selectIdx / 6 >= 5) selectIdx -= 30; else selectIdx += 6;
      tft.drawRect(30 + (selectIdx % 6) * 30, 55 + (selectIdx / 6) * 35, 28, 33, ILI9341_YELLOW);
    }
  }

  isIdle = isNowIdle;

  long currentTime = millis();
  if (currentTime > initialsTimeUp) {
    // user took too long to enter initials, go with whatever we have
    tft.fillRect(100, 260, 40, 40, ILI9341_BLACK);
    tft.setCursor(100, 260);
    tft.setTextSize(4);
    tft.setTextColor(ILI9341_RED);
    tft.print("0");
    updateHighScores(currInits);
    isInitials = false;
    currInitIdx = 0;
    currInits[0] = currInits[1] = currInits[2] = ' ';
    delay(1000);
    drawHighScoreScreen();
    return;
  }

  // update the countdown timer on screen using (initialsTimeUp - currentTime) / 1000
  currentTime = (initialsTimeUp - currentTime) / 1000;
  if (currentTime < secsLeft) {
    secsLeft = currentTime;
    tft.fillRect(100, 280, 50, 40, ILI9341_BLACK);
    tft.setCursor(100, 280);
    tft.setTextSize(4);
    tft.setTextColor(ILI9341_YELLOW);
    tft.print(secsLeft);
  }

  // delay a little bit
  delay(50);
}

// Sleep for 4 seconds, checking back every 100 ms to see whether
// the user is pressing the Z button.  Return true if Z was pressed.
boolean delayCheckZButton() {
  for (int delayCount = 0; delayCount < 200; delayCount++) {
    delay(20);
    if (nunchuk_zbutton()) return true;
  }
  return false;
}

void drawInitialsScreen() {
  tft.fillScreen(ILI9341_BLACK);

  // Static text at the top of the screen
  tft.setCursor(5, 0);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_YELLOW);
  tft.println(F("ENTER YOUR INITIALS"));

  // Boxes where selected initials will be displayed
  tft.drawRect( 65, 16, 28, 35, ILI9341_YELLOW);
  tft.drawRect(105, 16, 28, 35, ILI9341_YELLOW);
  tft.drawRect(145, 16, 28, 35, ILI9341_YELLOW);

  // Keyboard for the user to choose from (6x6, letters and numbers)
  tft.setTextSize(4);
  tft.setTextColor(ILI9341_WHITE);
  char nextChar = 'A';
  for (int y = 55; y < 265; y += 35) {
    for (int x = 30; x < 210; x += 30) {
      tft.fillRect(x, y, 28, 33, ILI9341_BLUE);
      tft.setCursor(x + 4, y + 4);
      tft.print(nextChar);
      if (nextChar == 'Z') nextChar = '0';
      else nextChar++;
    }
  }
  tft.drawRect(30, 55, 28, 33, ILI9341_YELLOW);
  selectIdx = 0;

  // Countdown timer
  tft.setCursor(100, 280);
  tft.setTextSize(4);
  tft.setTextColor(ILI9341_YELLOW);
  secsLeft = 30;
  tft.print(secsLeft);
  initialsTimeUp = millis() + 30999;
}

void drawGameOverScreen() {
  tft.fillRect(SCREEN_WIDTH/4, SCREEN_HEIGHT/2 - 50, SCREEN_WIDTH/2, 100, ILI9341_BLACK);
  tft.setCursor(SCREEN_WIDTH/4 + 10, SCREEN_HEIGHT/2 - 40);
  tft.setTextSize(5);
  tft.setTextColor(ILI9341_YELLOW);
  tft.println(F("Game"));
  tft.setCursor(SCREEN_WIDTH/4 + 10, SCREEN_HEIGHT/2);
  tft.println(F("Over"));
}

void drawPauseScreen() {
  tft.fillRect(LEFT_EDGE, TOP_EDGE, GRID_WIDTH*BLOCK_WIDTH, GRID_HEIGHT*BLOCK_HEIGHT, ILI9341_BLACK);
  tft.setCursor(SCREEN_WIDTH/8, SCREEN_HEIGHT/2 - 20);
  tft.setTextSize(5);
  tft.setTextColor(ILI9341_YELLOW);
  tft.println(F("Paused"));

}

void drawOnResume() {
  // Clear the "paused" message
  tft.fillRect(LEFT_EDGE, TOP_EDGE, GRID_WIDTH*BLOCK_WIDTH, GRID_HEIGHT*BLOCK_HEIGHT, ILI9341_BLACK);

  // Draw the board contents
  for (int j = 0; j < GRID_HEIGHT; j++) {
    for (int i = 0; i < GRID_WIDTH; i++) {
      int color = getBoardColorAt(j, i);
      if (color >  0) {
        tft.fillRect(LEFT_EDGE + BLOCK_WIDTH*i, TOP_EDGE + BLOCK_HEIGHT*j, BLOCK_WIDTH - 1, BLOCK_HEIGHT - 1, color);
      }
    }
  }

  // Draw the active piece
  drawActivePiece(true);
}

// Fully redraw the screen for a new game
void drawNewGameScreen() {
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(LEFT_EDGE - WALL_THICKNESS, TOP_EDGE - WALL_THICKNESS, WALL_THICKNESS, BLOCK_HEIGHT*GRID_HEIGHT + WALL_THICKNESS*2, ILI9341_WHITE);
  tft.fillRect(LEFT_EDGE + GRID_WIDTH*BLOCK_WIDTH, TOP_EDGE - WALL_THICKNESS, WALL_THICKNESS, BLOCK_HEIGHT*GRID_HEIGHT + WALL_THICKNESS*2, ILI9341_WHITE);
  tft.fillRect(LEFT_EDGE, TOP_EDGE - WALL_THICKNESS, BLOCK_WIDTH*GRID_WIDTH, WALL_THICKNESS, ILI9341_WHITE);
  tft.fillRect(LEFT_EDGE, TOP_EDGE + BLOCK_HEIGHT*12, BLOCK_WIDTH*GRID_WIDTH, WALL_THICKNESS, ILI9341_WHITE);
  tft.setCursor(LEFT_1UP, TOP_1UP);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_YELLOW);
  tft.println(F("1UP"));
  tft.setCursor(LEFT_HI, TOP_HI);
  tft.println(F("HIGH SCORE"));
}

void drawHighScoreScreen() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(LEFT_HI - 40, TOP_HI);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_YELLOW);
  tft.println(F("HIGH SCORES"));

  tft.setTextColor(ILI9341_WHITE);
  for (int i = 0; i < 10; i++) {
    tft.setCursor(HI_SCORE_LEFT - 60, HI_SCORE_TOP + 20 + (i * 20));
    tft.print(hsInitials[i*3]);
    tft.print(hsInitials[i*3 + 1]);
    tft.print(hsInitials[i*3 + 2]);
    tft.print(' ');
    drawScoreAtCursor(hsScores[i]);
  }
}

void initializeBoardState() {
  // Clear all pieces
  for (int i = 0; i < GRID_HEIGHT; i++) {
    boardState[i] = 0;
  }

  score = 0;
  loopsPerDrop = 80;
  placeNewPiece();
  isIdle = true;
}

unsigned int getBoardColorAt(int row, int col) {
  return asUint(bitRead(boardState[row], 29 - col*3) * 4 + bitRead(boardState[row], 28 - col*3) * 2 + bitRead(boardState[row], 27 - col*3));
}

void setBoardColorAt(int row, int col, unsigned int color) {
  byte colorAsByte = asByte(color);
  // Clear the affected three bits
  if (bitRead(colorAsByte, 2) == 1) bitSet(boardState[row], 29 - col*3); else bitClear(boardState[row], 29 - col*3);
  if (bitRead(colorAsByte, 1) == 1) bitSet(boardState[row], 28 - col*3); else bitClear(boardState[row], 28 - col*3);
  if (bitRead(colorAsByte, 0) == 1) bitSet(boardState[row], 27 - col*3); else bitClear(boardState[row], 27 - col*3);
}

void placeNewPiece() {
  // Place a random piece at the top of the board
  activePiece = random(28);
  piece_x = 4;
  piece_y = 0;
  drawActivePiece(true);
  loopsRemaining = loopsPerDrop;
}

//String pieces[28] = {  // 4 positions of 7 pieces
//  "14XXXX", "41XXXX", "14XXXX", "41XXXX",          // I
//  "23XXX  X", "32 X XXX", "23X  XXX", "32XXX X ",  // J
//  "23XXXX  ", "32XX X X", "23  XXXX", "32X X XX",  // L
//  "22XXXX", "22XXXX", "22XXXX", "22XXXX",          // O
//  "23 XXXX ", "32X XX X", "23 XXXX ", "32X XX X",  // S
//  "23XXX X ", "32 XXX X", "23 X XXX", "32X XXX ",  // T
//  "23XX  XX", "32 XXXX ", "23XX  XX", "32 XXXX "   // Z
//};

// convert color from ILI9341 format to byte (to save space)
byte asByte(unsigned int color) {
  switch (color) {
    case ILI9341_CYAN: return 1;
    case ILI9341_BLUE: return 2;
    case ILI9341_ORANGE: return 3;
    case ILI9341_YELLOW: return 4;
    case ILI9341_GREEN: return 5;
    case ILI9341_PURPLE: return 6;
    case ILI9341_RED: return 7;
  }
  return 0;
}

// convert color from byte to ILI9341 format (for display)
unsigned int asUint(byte color) {
  switch (color) {
    case 1: return ILI9341_CYAN;
    case 2: return ILI9341_BLUE;
    case 3: return ILI9341_ORANGE;
    case 4: return ILI9341_YELLOW;
    case 5: return ILI9341_GREEN;
    case 6: return ILI9341_PURPLE;
    case 7: return ILI9341_RED;
  }
  return ILI9341_BLACK;
}

unsigned int getPieceColor(int piece) {
  switch (piece / 4) {
    case 0: return ILI9341_CYAN;
    case 1: return ILI9341_BLUE;
    case 2: return ILI9341_ORANGE;
    case 3: return ILI9341_YELLOW;
    case 4: return ILI9341_GREEN;
    case 5: return ILI9341_PURPLE;
  }
  return ILI9341_RED;
}

int getPieceHeight(int piece) {
  switch (piece) {
    case 0: case 2: return 1;
    case 1: case 3: return 4;
    case 5: case 7: case 9: case 11: case 17: case 19: case 21: case 23: case 25: case 27: return 3;
  }
  return 2;
}

int getPieceWidth(int piece) {
  switch (piece) {
    case 1: case 3: return 1;
    case 0: case 2: return 4;
    case 4: case 6: case 8: case 10: case 16: case 18: case 20: case 22: case 24: case 26: return 3;
  }
  return 2;
}

// Returns true if the specified piece is solid at the specified position.
// Positions are encoded as (row * number of columns) + column, where zero is the first row and first column.
boolean isPieceAtPosition(int piece, int pos) {
  switch (piece) {
    case 4: return pos == 0 || pos == 1 || pos == 2 || pos == 5;
    case 5: case 22: return pos == 1 || pos == 3 || pos == 4 || pos == 5;
    case 6: return pos == 0 || pos == 3 || pos == 4 || pos == 5;
    case 7: case 20: return pos == 0 || pos == 1 || pos == 2 || pos == 4;
    case 9: return pos == 0 || pos == 1 || pos == 3 || pos == 5;
    case 10: return pos >= 2 && pos <= 5;
    case 11: return pos == 0 || pos == 2 || pos == 4 || pos == 5;
    case 16: case 18: case 25: case 27: return pos >= 1 && pos <= 4;
    case 17: case 19: return pos == 0 || pos == 2 || pos == 3 || pos == 5;
    case 21: return pos == 1 || pos == 2 || pos == 3 || pos == 5;
    case 23: return pos == 0 || pos == 2 || pos == 3 || pos == 4;
    case 24: case 26: return pos == 0 || pos == 1 || pos == 4 || pos == 5;
  }
  return pos >= 0 && pos <= 3;
}

void drawActivePiece(int show) {
  unsigned int color = (show ? getPieceColor(activePiece) : ILI9341_BLACK);
  int pieceHeight = getPieceHeight(activePiece);
  int pieceWidth = getPieceWidth(activePiece);
  for (int j = 0; j < pieceHeight; j++) {
    for (int i = 0; i < pieceWidth; i++) {
      if (isPieceAtPosition(activePiece, j*pieceWidth + i)) {
        tft.fillRect(LEFT_EDGE + (piece_x + i) * BLOCK_WIDTH, TOP_EDGE + (piece_y + j) * BLOCK_HEIGHT, BLOCK_WIDTH - 1, BLOCK_HEIGHT - 1, color);
      }
    }
  }
}

// Move the active piece left, right, or down, or rotate it.
// This function stops illegal movement (into other blocks).
void moveActivePiece(int delta_x, int delta_y) {
  // Check for collision with left edge of the board (illegal move)
  if (piece_x + delta_x < 0) return;

  int pieceHeight = getPieceHeight(activePiece);
  int pieceWidth = getPieceWidth(activePiece);

  // Check for collision with right edge of the board (illegal move)
  if (piece_x + delta_x + pieceWidth > GRID_WIDTH) return;

  // Check for collision with bottom of the board
  if (piece_y + delta_y + pieceHeight > GRID_HEIGHT) return;
  
  // If there is nothing in the way, move the piece
  if (!checkPieceCollision(activePiece, piece_x + delta_x, piece_y + delta_y)) {
    drawActivePiece(false);
    piece_x += delta_x;
    piece_y += delta_y;
    drawActivePiece(true);
  }

  if (checkBottom()) {
    addActivePieceToBoard();
    destroyRows();
    loopsRemaining = loopsPerDrop;
    placeNewPiece();
    stopDrop = true;
    if (checkBottom()) {
      // Game over
      drawGameOverScreen();
      isGameOver = true;
      delay(5000);
      if (score > hsScores[9]) {
        drawInitialsScreen();
        isInitials = true;
      }
    }
  }
}

// Rotate the active piece 90 degrees clockwise.
// This function stops the rotation if the result collides with blocks on the board.
void rotateActivePiece() {
  // Get info about the rotated piece
  int newPiece = activePiece + 1;
  if (newPiece % 4 == 0) newPiece -= 4;
  int pieceHeight = getPieceHeight(newPiece);
  int pieceWidth = getPieceWidth(newPiece);

  // If the rotated piece is off the board, stop the rotation
  if (piece_x + pieceWidth > GRID_WIDTH) return;
  if (piece_y + pieceHeight > GRID_HEIGHT) return;

  // If there is nothing in the way, rotate the piece
  if (!checkPieceCollision(newPiece, piece_x, piece_y)) {
    drawActivePiece(false);
    activePiece = newPiece;
    drawActivePiece(true);
  }

  if (checkBottom()) {
    addActivePieceToBoard();
    destroyRows();
    loopsRemaining = loopsPerDrop;
    placeNewPiece();
    stopDrop = true;
    if (checkBottom()) {
      // Game over
      drawGameOverScreen();
      isGameOver = true;
      delay(5000);
      if (score > hsScores[9]) {
        drawInitialsScreen();
        isInitials = true;
      }
    }
  }
}

// Before moving a piece to the left, right, or down, or rotating it, check
// whether the new position will cause a collision with other blocks on the board.
boolean checkPieceCollision(int piece, int new_piece_x, int new_piece_y) {
  int pieceHeight = getPieceHeight(piece);
  int pieceWidth = getPieceWidth(piece);
  for (int j = 0; j < pieceHeight; j++) {
    for (int i = 0; i < pieceWidth; i++) {
      if (isPieceAtPosition(piece, j*pieceWidth + i)) {
        if (getBoardColorAt(new_piece_y + j, new_piece_x + i) > 0) {
          // collision
          return true;
        }
      }
    }
  }
  return false;
}

boolean checkBottom() {
  // Check for bottom of the entire board
  int pieceHeight = getPieceHeight(activePiece);
  if (piece_y + pieceHeight >= 12) {
    return true;
  }

  // Check for solid objects under the lowest point of each column of the current piece
  return checkPieceCollision(activePiece, piece_x, piece_y + 1);
}

// Commit the blocks of the active piece to the board state.
void addActivePieceToBoard() {
  int pieceHeight = getPieceHeight(activePiece);
  int pieceWidth = getPieceWidth(activePiece);
  for (int j = 0; j < pieceHeight; j++) {
    for (int i = 0; i < pieceWidth; i++) {
      if (isPieceAtPosition(activePiece, j*pieceWidth + i)) {
        setBoardColorAt(piece_y + j, piece_x + i, getPieceColor(activePiece));
      }
    }
  }
}

void destroyRows() {
  long points = 0;
  boolean completeRow = false;
  for (int j = 0; j < GRID_HEIGHT; j++) {
    // Check for a full row of piece fragments
    completeRow = true;
    for (int i = 0; i < GRID_WIDTH; i++) {
      if (getBoardColorAt(j, i) == 0) {
        completeRow = false;
        break;
      }
    }

    if (completeRow) {
      // Score some points, and reward for multiple matches at once
      // 1 row = 10 pts, 2 rows = 30 pts, 3 rows = 70 pts, 4 rows = 150 pts
      points = points*2 + 10;

      // Animate removal of the row
      tft.fillRect(LEFT_EDGE, TOP_EDGE + j*BLOCK_HEIGHT, GRID_WIDTH*BLOCK_WIDTH, BLOCK_HEIGHT - 1, ILI9341_YELLOW);
      delay(100);
      tft.fillRect(LEFT_EDGE, TOP_EDGE + j*BLOCK_HEIGHT, GRID_WIDTH*BLOCK_WIDTH, BLOCK_HEIGHT - 1, ILI9341_WHITE);
      delay(100);
      tft.fillRect(LEFT_EDGE, TOP_EDGE + j*BLOCK_HEIGHT, GRID_WIDTH*BLOCK_WIDTH, BLOCK_HEIGHT - 1, ILI9341_YELLOW);
      delay(100);
      tft.fillRect(LEFT_EDGE, TOP_EDGE + j*BLOCK_HEIGHT, GRID_WIDTH*BLOCK_WIDTH, BLOCK_HEIGHT - 1, ILI9341_WHITE);
      delay(100);
      tft.fillRect(LEFT_EDGE, TOP_EDGE + j*BLOCK_HEIGHT, GRID_WIDTH*BLOCK_WIDTH, BLOCK_HEIGHT - 1, ILI9341_BLACK);
      // destroy the current row
      for (int n = j; n >= 1; n--) {
        boardState[n] = boardState[n - 1];
        for (int m = 0; m < GRID_WIDTH; m++) {
          tft.fillRect(LEFT_EDGE + m*BLOCK_WIDTH, TOP_EDGE + n*BLOCK_HEIGHT, BLOCK_WIDTH - 1, BLOCK_HEIGHT - 1, getBoardColorAt(n - 1, m));
        }
      }
      boardState[0] = 0;
      tft.fillRect(LEFT_EDGE, TOP_EDGE, GRID_WIDTH*BLOCK_WIDTH - 1, BLOCK_HEIGHT - 1, ILI9341_BLACK);
    }
  }
  updateScore(points);
}

void updateScore(long pts) {
  score += pts;
  if (score > 1000000) score -= 1000000;
  tft.fillRect(SCORE_LEFT, SCORE_TOP, SCORE_WIDTH, SCORE_HEIGHT, ILI9341_BLACK);
  tft.setCursor(SCORE_LEFT, SCORE_TOP);
  drawScoreAtCursor(score);

  if (score > highScore) {
    highScore = score;
    tft.fillRect(HI_SCORE_LEFT, HI_SCORE_TOP, HI_SCORE_WIDTH, HI_SCORE_HEIGHT, ILI9341_BLACK);
    tft.setCursor(HI_SCORE_LEFT, HI_SCORE_TOP);
    drawScoreAtCursor(highScore);
  }
}

void drawScoreAtCursor(int sc) {
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  if (sc < 100000) tft.print(' ');
  if (sc < 10000) tft.print(' ');
  if (sc < 1000) tft.print(' ');
  if (sc < 100) tft.print(' ');
  if (sc < 10) tft.print(' ');
  tft.println(sc);
}

// Load high scores from the file mtrishs.txt.
void initializeHighScores() {
  File hsFile = SD.open("mtrishs.txt");
  if (hsFile) {
    for (int i = 0; i < 10 && hsFile.available(); i++) {
      // read initials
      hsInitials[i*3] = hsFile.read();
      hsInitials[i*3 + 1] = hsFile.read();
      hsInitials[i*3 + 2] = hsFile.read();

      // read and skip colon
      hsFile.read();

      // read score
      hsScores[i] = 0;
      char digit = hsFile.read();
      while (digit >= '0' && digit <= '9') {
        hsScores[i] = hsScores[i] * 10 + (digit - '0');
        digit = hsFile.read();
      }
    }
    hsFile.close();
    highScore = hsScores[0];
  }
}

void updateHighScores(const char* inits) {
  if (score <= hsScores[9]) return;
  
  // move the current score up the high score chart
  for (int i = 8; i >= 0; i--) {
    if (score > hsScores[i]) {
      hsScores[i+1] = hsScores[i];
      hsInitials[i*3 + 3] = hsInitials[i*3];
      hsInitials[i*3 + 4] = hsInitials[i*3 + 1];
      hsInitials[i*3 + 5] = hsInitials[i*3 + 2];
    } else {
      hsScores[i+1] = score;
      hsInitials[i*3 + 3] = inits[0];
      hsInitials[i*3 + 4] = inits[1];
      hsInitials[i*3 + 5] = inits[2];
      break;
    }
  }
  if (score > hsScores[0]) {
    hsScores[0] = score;
    hsInitials[0] = inits[0];
    hsInitials[1] = inits[1];
    hsInitials[2] = inits[2];
  }

  // Write the updated high scores to the SD card.
  // If we lose power and restart later, we can load the updated high score table.
  SD.begin(SD_CS);
  SD.remove(F("mtrishs.txt"));
  File hsFile = SD.open(F("mtrishs.txt"), FILE_WRITE);
  for (int i = 0; i < 10; i++) {
    hsFile.print(hsInitials[i*3]);
    hsFile.print(hsInitials[i*3 + 1]);
    hsFile.print(hsInitials[i*3 + 2]);
    hsFile.print(':');
    hsFile.print(hsScores[i]);
    hsFile.print(' ');
  }
  hsFile.close();
}

void bmpDraw(char *filename, int16_t x, int16_t y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col, x2, y2, bx1, by1;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();

  if((x >= tft.width()) || (y >= tft.height())) return;

  Serial.println();
  Serial.print(F("Loading image '"));
  Serial.print(filename);
  Serial.println('\'');

  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.print(F("File not found"));
    return;
  }

  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print(F("File size: ")); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print(F("Header size: ")); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print(F("Image size: "));
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        x2 = x + bmpWidth  - 1; // Lower-right corner
        y2 = y + bmpHeight - 1;
        if((x2 >= 0) && (y2 >= 0)) { // On screen?
          w = bmpWidth; // Width/height of section to load/display
          h = bmpHeight;
          bx1 = by1 = 0; // UL coordinate in BMP file
          if(x < 0) { // Clip left
            bx1 = -x;
            x   = 0;
            w   = x2 + 1;
          }
          if(y < 0) { // Clip top
            by1 = -y;
            y   = 0;
            h   = y2 + 1;
          }
          if(x2 >= tft.width())  w = tft.width()  - x; // Clip right
          if(y2 >= tft.height()) h = tft.height() - y; // Clip bottom
  
          // Set TFT address window to clipped image bounds
          tft.startWrite(); // Requires start/end transaction now
          tft.setAddrWindow(x, y, w, h);
  
          for (row=0; row<h; row++) { // For each scanline...
  
            // Seek to start of scan line.  It might seem labor-
            // intensive to be doing this on every line, but this
            // method covers a lot of gritty details like cropping
            // and scanline padding.  Also, the seek only takes
            // place if the file position actually needs to change
            // (avoids a lot of cluster math in SD library).
            if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
              pos = bmpImageoffset + (bmpHeight - 1 - (row + by1)) * rowSize;
            else     // Bitmap is stored top-to-bottom
              pos = bmpImageoffset + (row + by1) * rowSize;
            pos += bx1 * 3; // Factor in starting column (bx1)
            if(bmpFile.position() != pos) { // Need seek?
              tft.endWrite(); // End TFT transaction
              bmpFile.seek(pos);
              buffidx = sizeof(sdbuffer); // Force buffer reload
              tft.startWrite(); // Start new TFT transaction
            }
            for (col=0; col<w; col++) { // For each pixel...
              // Time to read more pixel data?
              if (buffidx >= sizeof(sdbuffer)) { // Indeed
                tft.endWrite(); // End TFT transaction
                bmpFile.read(sdbuffer, sizeof(sdbuffer));
                buffidx = 0; // Set index to beginning
                tft.startWrite(); // Start new TFT transaction
              }
              // Convert pixel from BMP to TFT format, push to display
              b = sdbuffer[buffidx++];
              g = sdbuffer[buffidx++];
              r = sdbuffer[buffidx++];
              tft.writePixel(tft.color565(r,g,b));
            } // end pixel
          } // end scanline
          tft.endWrite(); // End last TFT transaction
        } // end onscreen
        Serial.print(F("Loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  if(!goodBmp) Serial.println(F("BMP format not recognized."));
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

