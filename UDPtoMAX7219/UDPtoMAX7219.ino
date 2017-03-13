/**
 * https://github.com/terokorp/Arduino-Max7219-scrolling-text 
 * 
 **/

#include <stdint.h>
#include <avr/pgmspace.h>

#include <SPI.h>
#include <Ethernet.h>
#define UDP_TX_PACKET_MAX_SIZE 860 //increase UDP size

#include "LedControlSPI.h"
//#include "LedControl.h"
#include "font5x7.h"
#include "font5x7extra.h"

#define SCLK       13   // SCLK, shared with ethernet shield
#define MISO       12   // MISO 
#define MOSI       11   // MOSI, shared with ethernet shield
#define SS_E       10   // for enthenet shield
#define SS_D       9    // for display

#define numDevices 4    // roundup( countof displaymodules / 4)

unsigned int scrollDelay = 20;
unsigned int localPort = 8888;
byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };

#ifdef LedControlSPI_h
LedControl lc = LedControl(SS_D, numDevices * 4);
#else
LedControl lc = LedControl(MOSI, SCLK, SS_D, numDevices * 4);
#endif
EthernetClient client;

EthernetUDP Udp;
char buf[UDP_TX_PACKET_MAX_SIZE + 2] = {0};
unsigned long displaybuffer [(numDevices + 2) * 8] = {0}; // Display pixel data
unsigned char ch[4] = {0};  // Current UTF-8 char

int location = 0; // Location on message
int count = 0;    // Remaining pixels count on scroll

void setup() {
  for (int x = 0; x < numDevices * 4; x++) {
    lc.shutdown(x, false);      // The MAX72XX is in power-saving mode on startup
    lc.setIntensity(x, 0);      // Set the brightness to default value 0-15
    lc.clearDisplay(x);         // and clear the display
  }

  //Ethernet.begin(mac, ip, myDns, gateway, subnet);
  Serial.begin(115200);

  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
  }
  
  Serial.print("Displays: ");
  Serial.println(lc.getDeviceCount());

  // Show ip on startup
  uint32_t ipAddress = Ethernet.localIP();
  uint8_t *ipPtr = (uint8_t*) &ipAddress;
  sprintf(buf, "%d.%d.%d.%d:%d   \0", ipPtr[0], ipPtr[1], ipPtr[2], ipPtr[3], localPort);
  Udp.begin(localPort);
}


void loop() {
  if (location <= 0 && count <= 0) {
    int packetSize = Udp.parsePacket();
    if (packetSize) {
      Udp.read(buf, UDP_TX_PACKET_MAX_SIZE);
      Serial.println(buf);
      buf[packetSize] = 0x20;
      buf[packetSize + 1] = 0;
    }
    packetSize = Udp.parsePacket();
    if (packetSize) {
      Serial.println("Too many packets: Skipping!");
    }
  }

  scrollMessageMem(buf);

  delay(scrollDelay);
}

void scrollMessageMem(unsigned char * messageString) {
  //Great tool: www.utf8-chartable.de/

  if (count <= 0) {  // Load next char if no more pixels left
    parsechar(messageString + location, ch);
    loadChar(ch);
  }

  for (int x = numDevices; x >= 0; x--) {
    UpdateDisplayLong(x);
  }

  scroll();
  count--;
}

char parsechar(unsigned char * messageString, unsigned char * ch) {
  //codes 0x01-0x7f are identical in ASCII and UTF8
  //codes 0xA0-0xBF in ISO-8859-1 and Windows-1252 are two-byte characters in UTF-8 -- 0xC2 as a first byte, the second byte is identical to the extended ASCII-code.
  //codes 0xC0-0xFF in ISO-8859-1 and Windows-1252 are two-byte characters in UTF-8 -- 0xC3 as a first byte, the second byte differs only in the first two bits.
  //codes 0x80-0x9F in Windows-1252 are different, but usually only the €-symbol will be needed from this range. The euro symbol is 0x80 in Windows-1252, 0xa4 in ISO-8859-15, and 0xe2 0x82 0xac in UTF-8

  char len = 0; // Character lenght(bytes)
  memcpy(ch, messageString, 4);
  Serial.print("parsing: ");
  printdebug(ch);
  memset(ch, 0x00, 4); // Clear current char bytes


  unsigned char c = messageString[0];
  if (c == 0) {
    location = 0;
    count = 0;
    return;
  }
  else if (c >= 0x01 && c <= 0x7f) {  //U+0000 to U+007F are 1 byte
    len = 1;
    memcpy(ch, messageString, len);
  }
  else if (c >= 0xC2 && c <= 0xDF) {  // U+0080 to U+07FF are 2 bytes
    len = 2;
    memcpy(ch, messageString, len);
    if (c == 0xC4) { // Ä
      len = 1;
      ch[0] = 0xc3;
      ch[1] = 0x84;
      ch[2] = 0x00;
      ch[3] = 0x00;
    }
    if (c == 0xC5) { // Å
      len = 1;
      ch[0] = 0xc3;
      ch[1] = 0x85;
      ch[2] = 0x00;
      ch[3] = 0x00;
    }
    if (c == 0xD6) { // Ö
      len = 1;
      ch[0] = 0xc3;
      ch[1] = 0x96;
      ch[2] = 0x00;
      ch[3] = 0x00;
    }
  }
  else if (c >= 0xE0 && c <= 0xEF) { // U+0800 to U+FFFF are 3 bytes
    len = 3;
    memcpy(ch, messageString, len);
    if (c == 0xE5) { // å
      len = 1;
      ch[0] = 0xc3;
      ch[1] = 0xa5;
      ch[2] = 0x00;
      ch[3] = 0x00;
    }
    if (c == 0xE4) { // ä
      len = 1;
      ch[0] = 0xc3;
      ch[1] = 0xa4;
      ch[2] = 0x00;
      ch[3] = 0x00;
    }
  }
  else if (c >= 0xF0) { // U+010000 to U+10FFFF are 4 bytes
    len = 4;
    memcpy(ch, messageString, len);
    if (c == 0xF6) { // ö
      len = 1;
      ch[0] = 0xc3;
      ch[1] = 0xb6;
      ch[2] = 0x00;
      ch[3] = 0x00;
    }
  } else {
    Serial.print("Oh no! ");
    Serial.println(c, HEX);
    len = 1;
  }
  location = location + len;

  Serial.print("parsed:  ");
  printdebug(ch);
  Serial.println("-----------");

}

void loadChar(unsigned char * ch) { // ch = UTF-8 character
  count = -1;
  if (!ch[0] && !ch[1] && !ch[2] && !ch[3]) count = 0; // End of string
  if (ch[0] == 0x0A) count = 8; // 0x0A = newline;
  if (ch[0] == 0xC2 && ch[1] == 0xB0) ch[0] = 0x7F;  // replacing 0xB0 (degree char) with 0x7F (degree char in font)

  if (ch[0] >= 0x20 && ch[0] <= 0x7f) {
    loadBuffer(font5x7, ch[0] - 0x20, 1);
  }
  else if (ch[0] == 0xC2) {
  }
  else if (ch[0] == 0xC3) {
    switch (ch[1]) {
      case 0x84: //C3 84 == Ä
        loadBuffer(font5x7extra, 4, 0); // dots
        loadBuffer(font5x7, char('A') - 0x20, 1);
        break;
      case 0x85: //C3 85 == Å
        loadBuffer(font5x7extra, 5, 0); // circle
        loadBuffer(font5x7, char('A') - 0x20, 1);
        break;
      case 0x96: //C3 96 == Ö
        loadBuffer(font5x7extra, 4, 0); // dots
        loadBuffer(font5x7, char('O') - 0x20, 1);
        break;

      case 0xA0: //C3 A0 == à
        loadBuffer(font5x7extra, 0, 0); // `
        loadBuffer(font5x7, char('a') - 0x20, 1);
        break;
      case 0xA1: //C3 A1 == á
        loadBuffer(font5x7extra, 1, 0); // ´
        loadBuffer(font5x7, char('a') - 0x20, 1);
        break;
      case 0xA2: //C3 A2 == â
        loadBuffer(font5x7extra, 2, 0); // ^
        loadBuffer(font5x7, char('a') - 0x20, 1);
        break;
      case 0xA3: //C3 A3 == ã
        loadBuffer(font5x7extra, 3, 0); // ~
        loadBuffer(font5x7, char('a') - 0x20, 1);
        break;
      case 0xA4: //C3 A4 == ä
        loadBuffer(font5x7extra, 4, 1); // dots
        loadBuffer(font5x7, char('a') - 0x20, 1);
        break;
      case 0xA5: //C3 A5 == å
        loadBuffer(font5x7extra, 5, 1); // circle
        loadBuffer(font5x7, char('a') - 0x20, 1);
        break;

      case 0xA8: //C3 A8 == è
        loadBuffer(font5x7extra, 0, 0); // `
        loadBuffer(font5x7, char('e') - 0x20, 1);
        break;
      case 0xA9: //C3 A9 == é
        loadBuffer(font5x7extra, 1, 0); // ´
        loadBuffer(font5x7, char('e') - 0x20, 1);
        break;
      case 0xAA: //C3 AA == ê
        loadBuffer(font5x7extra, 2, 0); // ^
        loadBuffer(font5x7, char('e') - 0x20, 1);
        break;
      case 0xAB: //C3 AB == ë
        loadBuffer(font5x7extra, 4, 1); // dots
        loadBuffer(font5x7, char('e') - 0x20, 1);
        break;

      case 0xAC:
        loadBuffer(font5x7extra, 0, 0); // `
        loadBuffer(font5x7, char('i') - 0x20, 1);
        break;
      case 0xAD:
        loadBuffer(font5x7extra, 1, 0); // ´
        loadBuffer(font5x7, char('i') - 0x20, 1);
        break;
      case 0xAE:
        loadBuffer(font5x7extra, 2, 0); // ^
        loadBuffer(font5x7, char('i') - 0x20, 1);
        break;

      case 0xB2: //C3 B2 == ò
        loadBuffer(font5x7extra, 0, 0); // `
        loadBuffer(font5x7, char('o') - 0x20, 1);
        break;
      case 0xB3: //C3 B3 == ó
        loadBuffer(font5x7extra, 1, 0); // ´
        loadBuffer(font5x7, char('o') - 0x20, 1);
        break;
      case 0xB4: //C3 B4 == ô
        loadBuffer(font5x7extra, 2, 0); // ^
        loadBuffer(font5x7, char('o') - 0x20, 1);
        break;
      case 0xB5: //C3 B5 == õ
        loadBuffer(font5x7extra, 3, 0); // ~
        loadBuffer(font5x7, char('o') - 0x20, 1);
        break;
      case 0xB6: //C3 B6 == ö
        loadBuffer(font5x7extra, 4, 1); // dots
        loadBuffer(font5x7, char('o') - 0x20, 1);
        break;
    }
  }
  else if (ch[0] == 0xE2) {
    if (ch[1] == 0x80)
      switch (ch[2]) {
        case 0x9C: // E2 80 9C == “
        case 0x9D: // E2 80 9D == ”
          loadBuffer(font5x7, char('"') - 0x20, 1);
          break;
        case 0x93: // E2 80 93 == –
          loadBuffer(font5x7, char('-') - 0x20, 1);
          break;
        case 0xA6: // E3 80 A6 == …
          loadBuffer(font5x7extra, 6, 1);
          break;
      }
    else if (ch[1] == 0x82 && ch[2] == 0xAC) { // E2 82 AC == €
      loadBuffer(font5x7extra, 7, 1);
    }
  }

  if (count < 0) {
    loadBuffer(font5x7, char('_') - 0x20, 1);

    Serial.print("Unknow char: ");
    printdebug(ch);
  }
  if (count < 0) count = 0;
  if (count > 10) count = 10;
}

// Load character into scroll buffer
void loadBuffer(const unsigned char font [] PROGMEM , char c, char shift) {
  for (int a = 0; a < 7; a++) { // Loop 7 times for a 5x7 font
    unsigned long x = pgm_read_byte_near(font + (c * 8) + a);
    displaybuffer[a + shift] = displaybuffer[a + shift] | x;
  }
  count = pgm_read_byte_near(font + (c * 8) + 7);    // Index into character table for kerning data
}

void UpdateDisplayLong(unsigned char section) {
  for (int a = 0; a < 8; a++) {
    unsigned long x = displaybuffer [8 * section + a];

    lc.setRow(section * 4 + 2, a, (x >> 24) );
    lc.setRow(section * 4 + 1, a, (x >> 16) );
    lc.setRow(section * 4 + 0, a, (x >> 8 ) );
    if (section != 0)
      lc.setRow(section * 4 - 1, a, (x)       );
  }
}

void scroll(void) {
  int x = 0;
  for (int x = numDevices; x >= 0; x--) {
    for (byte a = 0; a <= 7; a++) {
      char c = bitRead(displaybuffer[(x * 8) + a], sizeof(unsigned long) * 8 - 1);
      displaybuffer[(x * 8) + a] = (displaybuffer[(x * 8) + a] << 1);
      displaybuffer[(x * 8) + a + 8] = displaybuffer[(x * 8) + a + 8] | c;
    }
  }
}

void printdebug(unsigned char * b) {
  for (int x = 0; x <= 3; x++) {
    char tmp[16];
    sprintf(tmp, "%.2X", b[x]);
    Serial.print(tmp);
    Serial.print(" ");
  }
  Serial.print("(");
  for (int x = 0; x <= 3; x++) {
    if (b[x] != 0x00) Serial.print(char(b[x]));
  }
  Serial.print(")");
  Serial.println("");
}



