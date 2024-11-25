#include <SoftwareSerial.h>

// Define RX and TX pins
#define RX_PIN 7
#define TX_PIN 8

// Create SoftwareSerial instance
SoftwareSerial mySerial(RX_PIN, TX_PIN);

void setup() {
  // Start the software serial communication
  mySerial.begin(115200); // Baud rate must match the sender
  // Optional: Start hardware serial for debugging
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  Serial.println("Ready to receive data...");
}

void loop() {
  // Check if data is available
  if (mySerial.available() > 0) {
    int receivedData = mySerial.parseInt(); // Read the integer
    // Print received data to the Serial Monitor
    Serial.print("Received Data: ");
    Serial.println(Serial2.readString());
  }
}