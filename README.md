# UDPtoMAX7219
Arduino program which recieves UTF-8 messages with UDP and shows them on MAX7219 led matrix display.
Supports most ASCII characters, few extended ascii, and some other special characters.

## Bugs included:
* fail: íì
* missing: ï ùúûü ýÿ

## Requirements:
* Arduino UNO or better
* Arduino Ethernet Shield V1 or better
* MAX7219 led matrix display (program supposed to support up to 16 displays at default, 8 tested)

## Usage:
Send UTF-8 encoded message to address/port on display. eg.

    echo "This is test message ÅÄÖ åäö àáâãä èéêêë ìíîï òóôõö ùúûü ýÿ € " > /dev/udp/192.168.1.139/8888

# TwitterStream
Recieves twitter stream and sends tweets to UDPtoMAX7219

## Usage:
* Install requrements:
    cpan install AnyEvent::Twitter AnyEvent::Twitter::Stream
    cpan install Net::OAuth
    cpan install Config::Any

* copy config-example.json tp config.json
* edit your settings to it
* run

## Todo
* switch between users / tags.
