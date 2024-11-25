/*  Rui Santos & Sara Santos - Random Nerd Tutorials
    THIS EXAMPLE WAS TESTED WITH THE FOLLOWING HARDWARE:
      ESP32-2432S028R 2.8 inch 240×320 also known as the Cheap Yellow Display (CYD): https://makeradvisor.com/tools/cyd-cheap-yellow-display-esp32-2432s028r/
      SET UP INSTRUCTIONS: https://RandomNerdTutorials.com/cyd/
    The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
    Complete project details: https://RandomNerdTutorials.com/touchscreen-on-off-button-cheap-yellow-display-esp32-2432s028r/
*/

#include <SPI.h>

/*  Install the "TFT_eSPI" library by Bodmer to interface with the TFT Display - https://github.com/Bodmer/TFT_eSPI
    *** IMPORTANT: User_Setup.h available on the internet will probably NOT work with the examples available at Random Nerd Tutorials ***
    *** YOU MUST USE THE User_Setup.h FILE PROVIDED IN THE LINK BELOW IN ORDER TO USE THE EXAMPLES FROM RANDOM NERD TUTORIALS ***
    FULL INSTRUCTIONS AVAILABLE ON HOW CONFIGURE THE LIBRARY: https://RandomNerdTutorials.com/cyd/ */
#include <TFT_eSPI.h>
#include <SoftwareSerial.h> 

// Install the "XPT2046_Touchscreen" library by Paul Stoffregen to use the Touchscreen - https://github.com/PaulStoffregen/XPT2046_Touchscreen
// Note: this library doesn't require further configuration
#include <XPT2046_Touchscreen.h>

TFT_eSPI tft = TFT_eSPI();

TFT_eSPI_Button key[6];
// Touchscreen pins
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define FONT_SIZE 1

// Button position and size
#define FRAME_X 60
#define FRAME_Y 60
#define FRAME_W 200
#define FRAME_H 120

// Red zone size
#define REDBUTTON_X FRAME_X
#define REDBUTTON_Y FRAME_Y
#define REDBUTTON_W (FRAME_W / 2)
#define REDBUTTON_H FRAME_H

// Green zone size
#define GREENBUTTON_X (REDBUTTON_X + REDBUTTON_W)
#define GREENBUTTON_Y FRAME_Y
#define GREENBUTTON_W (FRAME_W / 2)
#define GREENBUTTON_H FRAME_H

// RGB LED Pins
#define CYD_LED_BLUE 17
#define CYD_LED_RED 4
#define CYD_LED_GREEN 16
#define AA_FONT_LARGE "NotoSansBold36"


#define CYD_RX 3
#define CYD_TX 1 

// Touchscreen coordinates: (x, y) and pressure (z)
int x, y, z;

// Stores current button state
bool buttonState = false;

// SoftwareSerial mySerial(CYD_RX, CYD_TX);

HardwareSerial mySerial(1);


// Print Touchscreen info about X, Y and Pressure (Z) on the Serial Monitor
void printTouchToSerial(int touchX, int touchY, int touchZ) {
  Serial.print("X = ");
  Serial.print(touchX);
  Serial.print(" | Y = ");
  Serial.print(touchY);
  Serial.print(" | Pressure = ");
  Serial.print(touchZ);
  Serial.println();
}

// Draw button frame
void drawFrame() {
  tft.drawRect(FRAME_X, FRAME_Y, FRAME_W, FRAME_H, TFT_BLACK);
}

// Draw a red button
void drawRedButton() {
  tft.fillRect(REDBUTTON_X, REDBUTTON_Y, REDBUTTON_W, REDBUTTON_H, TFT_RED);
  tft.fillRect(GREENBUTTON_X, GREENBUTTON_Y, GREENBUTTON_W, GREENBUTTON_H, TFT_WHITE);
  drawFrame();
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(FONT_SIZE);
  tft.setTextDatum(MC_DATUM);

  // tft.loadFont(AA_FONT_SMALL);
  tft.drawString("Tea 1", GREENBUTTON_X + (GREENBUTTON_W / 2), GREENBUTTON_Y + (GREENBUTTON_H / 2));
  buttonState = false;
}

// Draw a green button
void drawGreenButton() {
  tft.loadFont(AA_FONT_LARGE);
  tft.fillRect(GREENBUTTON_X, GREENBUTTON_Y, GREENBUTTON_W, GREENBUTTON_H, TFT_GREEN);
  tft.fillRect(REDBUTTON_X, REDBUTTON_Y, REDBUTTON_W, REDBUTTON_H, TFT_WHITE);
  drawFrame();
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(FONT_SIZE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Tea 2", REDBUTTON_X + (REDBUTTON_W / 2) + 1, REDBUTTON_Y + (REDBUTTON_H / 2));
  buttonState = true;
}

void setup() {
  Serial.begin(115200);
  mySerial.begin(9600, SERIAL_8N1, CYD_RX, CYD_TX);  // UART setup
    // Start the SPI for the touchscreen and init the touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  // Set the Touchscreen rotation in landscape mode
  // Note: in some displays, the touchscreen might be upside down, so you might need to set the rotation to 3: touchscreen.setRotation(3);
  touchscreen.setRotation(1);

  // Start the tft display
  tft.init();
  tft.invertDisplay(1);
  // Set the TFT display rotation in landscape mode
  tft.setRotation(1);

  // Clear the screen before writing to it
  tft.fillScreen(TFT_BLACK);

  // Draw button
  // drawGreenButton();
  drawButtons(); 
  pinMode(CYD_LED_GREEN, OUTPUT);
  digitalWrite(CYD_LED_GREEN, LOW);
}

void drawButtons() {
  uint16_t bWidth = SCREEN_WIDTH/3;
  uint16_t bHeight = SCREEN_HEIGHT/2;
  // Generate buttons with different size X deltas
  for (int i = 0; i < 6; i++) {
    key[i].initButton(&tft,
                      bWidth * (i%3) + bWidth/2,
                      bHeight * (i/3) + bHeight/2,
                      bWidth,
                      bHeight,
                      TFT_BLACK, // Outline
                      TFT_BLUE, // Fill
                      TFT_BLACK, // Text
                      "",
                      1);

    key[i].drawButton(false, String(i+1));
  }
}


void loop() {
  // Checks if Touchscreen was touched, and prints X, Y and Pressure (Z) info on the TFT display and Serial Monitor
  TS_Point p = touchscreen.getPoint();
  x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
  y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
  z = p.z;


  for (uint8_t b = 0; b < 6; b++) {
    if ((z > 0) && key[b].contains(x, y)) {
      key[b].press(true);  // tell the button it is pressed
    } else {
      key[b].press(false);  // tell the button it is NOT pressed
    }
  }

  // Check if any key has changed state
  for (uint8_t b = 0; b < 6; b++) {
    // If button was just pressed, redraw inverted button
    if (key[b].justPressed()) {
      // Serial.printf("Button %d pressed\n", b);
      Serial.printf("Button %d pressed\n", b);
      mySerial.println(String(b));
      key[b].drawButton(true, String(b+1));

      // mySerial.println(b); // print the tea to the GPIO RX/TX. 
      // Serial2.println(String(b)); 
    }

    // If button was just released, redraw normal color button
    if (key[b].justReleased()) {
      // Serial.printf("Button %d released\n", b);
      Serial.println("Button " + (String)b + " released");
      key[b].drawButton(false, String(b+1));
    }
  }

  delay(50);
  // Serial.println(p.z);
  // printTouchToSerial(p);
}

// void SendCommand(teaVariety) { 

// }