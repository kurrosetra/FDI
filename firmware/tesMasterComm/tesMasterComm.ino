String msg = "";

void setup() {
  Serial.begin(38400);
  Serial.println(F("LRT master"));

  msg.reserve(128);
}
String defMsg = " send from master, with counter";

void loop() {
  static uint32_t sendTimer = 0;
  static uint8_t msgCounter = 0;
  static uint8_t defCounter = 0;
  static bool sendingActive = false;
  char c;

  if (millis() >= sendTimer && sendingActive) {
    sendTimer = millis() + 3000;

    Serial.print(F("$MSG,1,2,\""));
    msg = String(msgCounter++);
    msg += defMsg;
    c = 'a';
    while (msg.length() < 64) {
      if (c > 'z')
        c = 'a';
      msg += char(c++);
    }
    Serial.print(msg);
    Serial.println(F("\"*"));
  }

  if (Serial.available()) {
    c = Serial.read();

    if (c == 'd') {
      Serial.print(F("$DEF,7,2,\"default text #"));
      Serial.print(defCounter++);
      Serial.println(F("\"*"));
    }
    else if (c == 'b') {
      Serial.println(F("$BLK,2,5000*"));
    }
    else if (c == 'B') {
      Serial.println(F("$BLK,7,5000*"));
    }
    else if (c == '0') {
      Serial.println(F("$BLK,0,5000*"));
    }
    else if (c == 'A') {
      sendingActive = !sendingActive;
    }
    else if (c == 'i') {
      Serial.println(F("$MSG,2,1,\"Respati\r\nhw: 001 sw: 001\r\nIP: 001054:1234\r\nNET: 255000,001001\"*"));
    }
    else if (c == 'I') {
      Serial.println(F("$MSG,6,1,\"Respait\r\nhw: 2 sw: 3\r\nIP: 001103:1234\r\nNET: 255000,001001\"*"));
    }
  }
}
