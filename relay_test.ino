#define RELAY_PIN 19  // GPIO pin connected to the relay

void setup() {
  pinMode(RELAY_PIN, OUTPUT);  // Set the relay pin as an output
  digitalWrite(RELAY_PIN, LOW);  // Ensure the relay starts off
  Serial.begin(115200);
}

void loop() {
  digitalWrite(RELAY_PIN, HIGH);  // Turn the relay ON
  Serial.println("on");
  delay(3000);  // Wait for 2 seconds
  digitalWrite(RELAY_PIN, LOW);   // Turn the relay OFF
  Serial.println("off");
  delay(3000);  // Wait for 2 seconds
}
