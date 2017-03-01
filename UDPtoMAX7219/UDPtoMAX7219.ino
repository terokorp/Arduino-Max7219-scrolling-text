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
#define MOSI       11   // MOSI shared with ethernet shield
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
int location = 0; // Location on message
int count = 0;    // Remaining pixels count on scroll

void setup() {
  for (int x = 0; x < numDevices * 4; x++) {
    lc.shutdown(x, false);      // The MAX72XX is in power-saving mode on startup
    lc.setIntensity(x, 0);      // Set the brightness to default value
    lc.clearDisplay(x);         // and clear the display
  }

  //Ethernet.begin(mac, ip, myDns, gateway, subnet);
  Serial.begin(115200);

  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
  }

  // Show ip on startup
  uint32_t ipAddress = Ethernet.localIP();
  uint8_t *ipPtr = (uint8_t*) &ipAddress;
  sprintf(buf, "%d.%d.%d.%d:%d   \0", ipPtr[0], ipPtr[1], ipPtr[2], ipPtr[3], localPort);
  Udp.begin(localPort);
}


void loop() {
  if (location == 0 && count == 0) {
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

  //lc.setIntensity(1, 15);

  scrollMessageMem(buf);

  delay(scrollDelay);
}

void scrollMessageMem(unsigned char * messageString) {
  int c = 0;
  //Great tool: www.utf8-chartable.de/

  if (count <= 0) {  // Load next char if no more pixels left
    c = messageString[location];
    if (c == 0) {
      location = 0;
      count = 1;
    }
    else if (c >= 0x01 && c <= 0x7f) {  //U+0000 to U+007F are 1 byte
      loadChar(c, 0x00, 0x00);
      location = location + 1;
    }
    else if (c >= 0xC2 && c <= 0xDF) {  // U+0080 to U+07FF are 2 bytes
      loadChar(c, messageString[location + 1], 0x00);
      location = location + 2;
    }
    else if (c >= 0xE0 && c <= 0xEF) { // U+0800 to U+FFFF are 3 bytes
      loadChar(c, messageString[location + 1], messageString[location + 2]);
      location = location + 3;
    }
    else if (c >= 0xF0) { // U+010000 to U+10FFFF are 4 bytes
      location = location + 4;
      return;
    } else {
      Serial.print("Oh no! ");
      Serial.println(c, HEX);
      location++;
    }
  }

  for (int x = numDevices; x >= 0; x--) {
    printBufferLong(x);
  }

  scroll();
  count--;
}


void loadChar(int b1, int b2, int b3) {
  //codes 0x01-0x7f are identical in ASCII and UTF8
  //codes 0xA0-0xBF in ISO-8859-1 and Windows-1252 are two-byte characters in UTF-8 -- 0xC2 as a first byte, the second byte is identical to the extended ASCII-code.
  //codes 0xC0-0xFF in ISO-8859-1 and Windows-1252 are two-byte characters in UTF-8 -- 0xC3 as a first byte, the second byte differs only in the first two bits.
  //codes 0x80-0x9F in Windows-1252 are different, but usually only the €-symbol will be needed from this range. The euro symbol is 0x80 in Windows-1252, 0xa4 in ISO-8859-15, and 0xe2 0x82 0xac in UTF-8

  count = -1;
  if (b1 == 0x0A) count = 8; // 0x0A = newline;
  if (b1 == 0xC2 && b2 == 0xB0) b1 = 0x7F;  // replacing 0xB0 (degree char) with 0x7F (degree char in font)

  if (b1 >= 0x20 && b1 <= 0x7f) {
    loadBuffer(font5x7, b1 - 0x20, 1);
  }
  else if (b1 == 0xC2) {
  }
  else if (b1 == 0xC3) {
    switch (b2) {
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
  else if (b1 == 0xE2) {
    if (b2 == 0x80)
      switch (b3) {
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
    else if (b2 == 0x82 && b3 == 0xAC) { // E2 82 AC == €
      loadBuffer(font5x7extra, 7, 1);
    }
  }

  if (count < 0) {
    loadBuffer(font5x7, char('_') - 0x20, 1);

    Serial.print("Unknow char: ");
    Serial.print(b1, HEX);
    Serial.print(" ");
    Serial.print(b2, HEX);
    Serial.print(" ");
    Serial.print(b3, HEX);
    Serial.print(" (");
    Serial.print(char(b1));
    if (b2 != 0x00)Serial.print(char(b2));
    if (b3 != 0x00) Serial.print(char(b3));
    Serial.print(")");

    Serial.println("");
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

void printBufferLong(char section) {
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
