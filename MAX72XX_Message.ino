#include <ESP8266WiFi.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <PubSubClient.h>

#define PRINT(s, v) { Serial.print(F(s)); Serial.print(v); }
#define PRINTS(s)   { Serial.print(F(s)); }
#define MAX_DEVICES 8

MD_MAX72XX mx = MD_MAX72XX(MD_MAX72XX::ICSTATION_HW, D8, MAX_DEVICES);

const uint8_t MESG_SIZE = 255;
const uint8_t CHAR_SPACING = 1;
const uint8_t SCROLL_DELAY = 75;

char curMessage[MESG_SIZE];
char newMessage[MESG_SIZE];
bool newMessageAvailable = false;
int showCounter = 0;

void callback(char* topic, byte* payload, unsigned int length) {
  strcpy(newMessage, (char *)payload);
  newMessage[length] = '\0';
  newMessageAvailable = true;
}

WiFiClient ethClient;
PubSubClient client("broker.hivemq.com", 1883, callback, ethClient);
void reconnect() {
  while (!client.connected()) {
    randomSeed(analogRead(0));
    sprintf(newMessage, "MESSAGE%06ld", random(0,999999));
    Serial.println("Attempting MQTT connection with id " + String(newMessage));
    if (client.connect(newMessage)) {
      Serial.println("MQTT connected with id " + String(newMessage));
      strcpy(curMessage, "MQTT connected");
      client.subscribe("MESSAGE");
    } else {
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

const char *err2Str(wl_status_t code)
{
  switch (code)
  {
  case WL_IDLE_STATUS:    return("IDLE");           break; // WiFi is in process of changing between statuses
  case WL_NO_SSID_AVAIL:  return("NO_SSID_AVAIL");  break; // case configured SSID cannot be reached
  case WL_CONNECTED:      return("CONNECTED");      break; // successful connection is established
  case WL_CONNECT_FAILED: return("CONNECT_FAILED"); break; // password is incorrect
  case WL_DISCONNECTED:   return("CONNECT_FAILED"); break; // module is not configured in station mode
  default: return("??");
  }
}

void handleWiFi(void)
{
  static enum { S_IDLE, S_WAIT_CONN, S_READ } state = S_IDLE;
  static char szBuf[1024];
  static uint16_t idxBuf = 0;
  static uint32_t timeStart;

  switch (state)
  {
  case S_IDLE:   // initialize
    idxBuf = 0;
    state = S_WAIT_CONN;
    break;

  case S_WAIT_CONN:   // waiting for connection
    {
      timeStart = millis();
      state = S_READ;
    }
    break;
  default:  state = S_IDLE;
  }
}

uint8_t scrollDataSource(uint8_t dev, MD_MAX72XX::transformType_t t)
{
  static enum { S_IDLE, S_NEXT_CHAR, S_SHOW_CHAR, S_SHOW_SPACE } state = S_IDLE;
  static char *p;
  static uint16_t curLen, showLen;
  static uint8_t  cBuf[8];
  uint8_t colData = 0;

  switch (state)
  {
  case S_IDLE: // reset the message pointer and check for new message to load
    p = curMessage;      // reset the pointer to start of message
    if (newMessageAvailable)  // there is a new message waiting
    {
      strcpy(curMessage, newMessage); // copy it in
      newMessageAvailable = false;
      showCounter = 0;
    }
    if (showCounter > 4) {
      curMessage[0] = '\0';
    } else {
     showCounter++;
    }
    state = S_NEXT_CHAR;
    break;

  case S_NEXT_CHAR: // Load the next character from the font tablex
    if (*p == '\0')
      state = S_IDLE;
    else
    {
      showLen = mx.getChar(*p++, sizeof(cBuf) / sizeof(cBuf[0]), cBuf);
      curLen = 0;
      state = S_SHOW_CHAR;
    }
    break;

  case S_SHOW_CHAR: // display the next part of the character
    colData = cBuf[curLen++];
    if (curLen < showLen)
      break;

    // set up the inter character spacing
    showLen = (*p != '\0' ? CHAR_SPACING : (MAX_DEVICES*COL_SIZE)/2);
    curLen = 0;
    state = S_SHOW_SPACE;
    // fall through

  case S_SHOW_SPACE:  // display inter-character spacing (blank column)
    curLen++;
    if (curLen == showLen)
      state = S_NEXT_CHAR;
    break;

  default:
    state = S_IDLE;
  }

  return(colData);
}

void scrollText(void)
{
  static uint32_t	prevTime = 0;
  if (millis() - prevTime >= SCROLL_DELAY)
  {
    mx.transform(MD_MAX72XX::TSL);  // scroll along - the callback will load all the data
    prevTime = millis();      // starting point for next time
  }
}

void setup()
{
  Serial.begin(115200);
  PRINTS("\n[MD_MAX72XX Message Display]\nGets message from MQTT Server and shows it as scrolling text");

  mx.begin();
  mx.setShiftDataInCallback(scrollDataSource);

  curMessage[0] = newMessage[0] = '\0';
  PRINT("\nConnecting", "");

  WiFi.begin("ASUS-2.4G");

  while (WiFi.status() != WL_CONNECTED)
  {
    PRINT("\n", err2Str(WiFi.status()));
    delay(500);
  }
  PRINTS("\nWiFi connected");
}

void loop()
{
  handleWiFi();
  scrollText();

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
