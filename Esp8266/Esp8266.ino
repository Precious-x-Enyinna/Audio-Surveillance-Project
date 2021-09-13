#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <TimeLib.h>
#include <WiFiUdp.h>

#define MICROPHONE_PIN A0
#define AUDIO_BUFFER_MAX 8192

static const char ntpServerName[] = "us.pool.ntp.org";
const int timeZone = 1;     // Central European Time
WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

time_t getNtpTime();
void sendNTPpacket(IPAddress &address);

bool dayTime = true;
bool timeChange = false;
int threshold = 0;

String pageRendered;

int audioStartIdx = 0, audioEndIdx = 0;
uint16_t audioBuffer[AUDIO_BUFFER_MAX];
uint16_t txBuffer[AUDIO_BUFFER_MAX];

uint8_t powerLed = D7;
bool pLedState = LOW;
uint8_t opxnLed = D6;
bool oLedState = LOW;

const unsigned char Active_buzzer = 14;

bool noiseDetected = false;
unsigned long alarmWait = 1000 * 60;
unsigned long alarmStartTime;
unsigned long lastTimeCheck;
unsigned long lastBuzz;
const int interval = 1000;

const int alarmOnTime = 1000 * 10;
bool alarmOnState = false;
int alarmState = LOW;

unsigned long lastRead = micros();

WiFiClient audioClient;
WiFiClient checkClient;

const char* host = "192.168.199.73";
const uint16_t port = 8081;

//Your WiFi credentials.
// Set password
// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "f9jgukfXkML5rK54E6YQ_fDV1URsWLdi";

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "Starr";
char pass[] = "onetimepass";

BLYNK_WRITE(V1)
{
  int value = param.asInt(); // Get value as integer
  Serial.print("From Blynk ");
  Serial.println(value);
  if (value == 0) {
    Blynk.virtualWrite(V1, LOW);
    alarmOnState = false;
    noiseDetected = false;
  } else {
    Blynk.virtualWrite(V1, HIGH);
    alarmOnState = true;
    alarmStartTime = millis();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(MICROPHONE_PIN, INPUT);
  pinMode (Active_buzzer, OUTPUT) ;
  pinMode (powerLed, OUTPUT) ;
  pinMode (opxnLed, OUTPUT) ;


  Blynk.begin(auth, ssid, pass);

  Serial.print("WiFi connected with IP: ");
  Serial.println(WiFi.localIP());


  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(60 * 60);
  Serial.println("UDP server has set the system time");

  if (timeStatus() == timeSet) {
    if (hour() < 21 && hour() > 6) {
      dayTime = true;
    } else {
      dayTime = false;
    }
    timeChange = dayTime;
  }

  setThreshold(15 * 1000);
  // 1/8000th of a second is 125 microseconds
  lastRead = micros();
}

void loop() {
  Blynk.run();

  if (!audioClient.connected()) {
    digitalWrite(opxnLed, LOW);
    if (!checkClient.connected()) {
      digitalWrite(opxnLed, LOW);
      if (checkClient.connect(host, port)) {
        audioClient = checkClient;
        digitalWrite(opxnLed, HIGH);
        Serial.println("Connected to server");
      }
    }
  }

  if (timeStatus() == timeSet) {
    if (hour() < 21 && hour() > 6) {
      dayTime = true;
    } else {
      dayTime = false;
    }
  }

  if (timeChange != dayTime) {
    setThreshold(10 * 1000);
    timeChange = dayTime;
  }

  if (noiseDetected) {
    if (pageRendered == "truetrue" || pageRendered == "true") {
      Serial.print("Site opened!!!!");
      noiseDetected = false;
    } else if (millis() - lastTimeCheck >= alarmWait) {
      alarmStartTime = lastTimeCheck + alarmWait;
      if (!alarmOnState)
        alarmOnState = true;
    } else {
      if (audioClient.available()) {
        pageRendered = audioClient.readStringUntil('\r');
        Serial.print(pageRendered);
        //char pageRendered = static_cast<char>(client.read());
      }
    }
  }
  if (alarmOnState) {
    Blynk.virtualWrite(V1, HIGH);
    alarmOn(alarmStartTime);
  } else {
    Blynk.virtualWrite(V1, LOW);
    digitalWrite(Active_buzzer, LOW);
  }
  //listen for 100ms, taking a sample every 125us,
  //and then send that chunk over the network.
  listenAndSend(100);

}

void alarmOn(unsigned long alarmStart) {
  if (millis() - alarmStart >= alarmOnTime) {
    alarmOnState = false;
    noiseDetected = false;
    alarmState = LOW;
    Blynk.virtualWrite(V1, LOW);
    digitalWrite(Active_buzzer, alarmState);
    Serial.print("Alarm Off!!!!");
    alarmStart = 0;
  } else if (millis() - lastBuzz >= interval) {
    lastBuzz = millis();
    if (alarmState == LOW) {
      alarmState = HIGH;
    } else {
      alarmState = LOW;
    }
    digitalWrite(Active_buzzer, alarmState);
  }
}

//Threshold configuration function
void setThreshold(int wait) {
  Serial.print("Configuring System");
  unsigned long startedListening = millis();
  while ((millis() - startedListening) < wait) {
    uint16_t currVal = analogRead(MICROPHONE_PIN);

    if (currVal > threshold) {
      threshold = currVal;
    }
    Serial.print(" curr val: ");
    Serial.print(currVal * 40);
    digitalWrite(powerLed, HIGH);
    delay(500);
    digitalWrite(powerLed, LOW);
    delay(500);

  }
  if (dayTime) {
    threshold += 5;
  } else {
    threshold += 2;
  }
  threshold *= 40;
  digitalWrite(powerLed, HIGH);

  Serial.print("Threshold is set at ");
  Serial.print(threshold);
  Serial.println(dayTime ? "During Day time" : "During Night time");
}

void listenAndSend(int delay) {
  unsigned long startedListening = millis();

  while ((millis() - startedListening) < delay) {
    unsigned long time = micros();

    if (lastRead > time) {
      // time wrapped?
      //lets just skip a beat for now, whatever.
      lastRead = time;
    }

    //125 microseconds is 1/8000th of a second
    if ((time - lastRead) > 125) {
      lastRead = time;
      readMic();
    }
  }
  sendAudio();
}

// Callback for Timer 1
void readMic(void) {
  uint16_t value = analogRead(MICROPHONE_PIN);
  if (audioEndIdx >= AUDIO_BUFFER_MAX) {
    audioEndIdx = 0;
  }
  audioBuffer[audioEndIdx++] = value * 40;
}

void copyAudio(uint16_t *bufferPtr) {
  //if end is after start, read from start->end
  //if end is before start, then we wrapped, read from start->max, 0->end

  int endSnapshotIdx = audioEndIdx;
  bool wrapped = endSnapshotIdx < audioStartIdx;
  int endIdx = (wrapped) ? AUDIO_BUFFER_MAX : endSnapshotIdx;
  int c = 0;

  for (int i = audioStartIdx; i < endIdx; i++) {
    // do a thing
    bufferPtr[c++] = audioBuffer[i];
  }

  if (wrapped) {
    //we have extra
    for (int i = 0; i < endSnapshotIdx; i++) {
      // do more of a thing.
      bufferPtr[c++] = audioBuffer[i];
    }
  }
  //and we're done.
  audioStartIdx = audioEndIdx;

  if (c < AUDIO_BUFFER_MAX) {
    bufferPtr[c] = -1;
  }
}

// Callback for Timer 1
void sendAudio(void) {
  copyAudio(txBuffer);

  int i = 0;
  uint16_t val = 0;

  if (audioClient.connected()) {
    digitalWrite(opxnLed, HIGH);
    write_socket(audioClient, txBuffer);
  }
  else {
    digitalWrite(opxnLed, LOW);
    while ( (val = txBuffer[i++]) < 65535 ) {
      Serial.print(val);
      Serial.print(',');
    }
    Serial.println("DONE");
  }
}

// an audio sample is 16bit, we need to convert it to bytes for sending over the network
void write_socket(WiFiClient socket, uint16_t *buffer) {
  int i = 0;
  uint16_t val = 0;

  int tcpIdx = 0;
  uint8_t tcpBuffer[1024];

  while ( (val = buffer[i++]) < 65535 ) {
    if (val > threshold && !noiseDetected && !alarmOnState && millis() - alarmStartTime > alarmWait) {
      Blynk.notify("noise detected");
      noiseDetected = true;
      lastTimeCheck = millis();
    }
    if ((tcpIdx + 1) >= 1024) {
      socket.write(tcpBuffer, tcpIdx);
      tcpIdx = 0;
    }
    tcpBuffer[tcpIdx] = val & 0xff;
    tcpBuffer[tcpIdx + 1] = (val >> 8);
    tcpIdx += 2;
  }

  // any leftovers?
  if (tcpIdx > 0) {
    socket.write(tcpBuffer, tcpIdx);
  }
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
