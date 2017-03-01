# UDPtoMAX7219
Arduino program which recieves UTF-8 messages with UDP and shows them on MAX7219 led matrix display.
Supports most ASCII characters, few extended ascii, and some other special characters.

## Bugs included:
* fail: íì
* missing: ï ùúûü ıÿ

## Requirements:
* Arduino UNO or better
* Arduino Ethernet Shield V1 or better
* MAX7219 led matrix display (program supposed to support up to 16 displays at default, 8 tested)

## Usage:
Send UTF-8 encoded message to address/port on display. eg.

    echo "This is test message ÅÄÖ åäö àáâãä èéêêë ìíîï òóôõö ùúûü ıÿ € " > /dev/udp/192.168.1.139/8888
