/* vim: set ts=2 sw=2 sts=2 et! : */
//
// BoilerMake Fall 2014 Badge Code
//
// These boards are equivalent to an Arduino Leonardo
// And contain an ATMega32U4 microprocessor and an nRF24L01 radio
//
// Lior Ben-Yehoshua (admin@Lior9999.com)
// Viraj Sinha       (virajosinha@gmail.com)
// Scott Opell       (me@scottopell.com)
//xz
// 10/15/2014
//
// Happy Hacking!
//

#define DEBUG                 1
#define MAX_TERMINAL_LINE_LEN 40
#define MAX_TERMINAL_WORDS    7

#define PAYLOAD_CACHE_SIZE    5

// 14 is strlen("send FFFF -m ")
// the max message length able to be sent from the terminal is
// total terminal line length MINUS the rest of the message
#define MAX_TERMINAL_MESSAGE_LEN  MAX_TERMINAL_LINE_LEN - 14

// Maps commands to integers
#define PING   0 // Ping
#define LED    1 // LED pattern
#define MESS   2 // Message
#define DEMO   3 // Demo Pattern

#define SROE_PIN    3 // using digital pin 3 for SR !OE
#define SRLATCH_PIN 8 // using digital pin 4 for SR latch

#define MULTI_ADDR  0x2bc0 // multi cast address
#define CHANNEL     30 // radio channel

#include <RF24.h>
#include <SPI.h>
#include <EEPROM.h>

typedef struct Payload {
  byte payload_id; // random number
  byte command; // command
  uint16_t address; // address of node Payload should be delivered to
  char data[28]; // data for command
} Payload;

void welcomeMessage(void);
void printHelpText(void);
void printPrompt(void);

void networkRead(void);
void serialRead(void);
void handleSerialData(char[], byte);
void sendPacket(struct Payload * , uint8_t, bool origin = false);

bool isInCache(int, int *, size_t);
uint8_t addToPayloadCache(byte);

void setValue(word);
void handlePayload(struct Payload *);

void ledDisplay(byte);
void displayDemo();
void printPayloadCache(void);

// Global Variables
boolean terminalConnect = false; // indicates if the terminal has connected to the board yet
int payload_cache[PAYLOAD_CACHE_SIZE];
int payload_cache_index = 0;

// nRF24L01 radio static initializations
RF24 radio(9,10); // Setup nRF24L01 on SPI bus using pins 9 & 10 as CE & CSN, respectively

// Read this node's address from EEPROM
const uint16_t this_node_address = (EEPROM.read(0) << 8) | EEPROM.read(1);

// This runs once on boot
void setup() {
  Serial.begin(9600);

  // SPI initializations
  SPI.begin();
  SPI.setBitOrder(MSBFIRST); // nRF requires MSB first
  SPI.setClockDivider(16); // Set SPI clock to 16 MHz / 16 = 1 MHz

  // nRF radio initializations
  radio.begin();
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_1MBPS); // 1Mbps transfer rate
  radio.setAutoAck(false); // Disable ACKs to prevent multicast ACK flooding
  radio.setCRCLength(RF24_CRC_16); // 16-bit CRC
  radio.setChannel(CHANNEL); // Channel center frequency = 2.4005 GHz + (Channel# * 1 MHz)
  radio.openReadingPipe(0, MULTI_ADDR); // multicast address
  radio.startListening(); // Start listening on opened address

  // Shift register pin initializations
  pinMode(SROE_PIN, OUTPUT);
  pinMode(SRLATCH_PIN, OUTPUT);
  digitalWrite(SROE_PIN, HIGH);
  digitalWrite(SRLATCH_PIN, LOW);

  // Initialize payload cache
  for(int i = 0; i < PAYLOAD_CACHE_SIZE; i++) {
    payload_cache[i] = 0;
  }

  // Seed PRNG
  randomSeed(analogRead(0));

  // Display welcome message
  welcomeMessage();

  // make the pretty LEDs happen
  ledDisplay(2);
}


// This loops forever
void loop() {
  // Displays welcome message if serial terminal connected after program setup
  if (Serial && !terminalConnect) { 
    welcomeMessage();
    terminalConnect = true;
  } else if (!Serial && terminalConnect) {
    terminalConnect = false;
  }

  networkRead(); // Read from network
  serialRead(); // Read from serial
}


// Handle reading from the radio
void networkRead() {
  digitalWrite(SROE_PIN, LOW);
  setValue(0x0100);
  delay(1);
  while (radio.available()) {
    setValue(0x0300);
    delay(1);
    Payload * current_payload = (Payload *) malloc(sizeof(Payload));
    current_payload->address = 0;

    // Fetch the payload
    radio.read( current_payload, sizeof(Payload) );
    handlePayload(current_payload);
  }
  setValue(0x0000);
  digitalWrite(SROE_PIN,HIGH);
}


// Get user input from serial terminal
void serialRead() {
  char inData[MAX_TERMINAL_LINE_LEN]; // allocate space for incoming serial string
  byte index = 0; // Index into array, where to store chracter
  char inChar; // Where to store the character to read

  // fills up characters from the terminal, leaving room for null terminator
  while ( index < MAX_TERMINAL_LINE_LEN - 1 && Serial.available() > 0){
    inChar = Serial.read();
    if (inChar == '\r'){
      inData[index] = '\0';
      break;
    } else {
      inData[index] = inChar;
      index++;
    }
  }

  if (index > 0){ // if we read some data, then process it
    Serial.println(inData);
    handleSerialData(inData, index);
    printPrompt();
  }

}


// Handle received commands from user obtained via the serial termina
void handleSerialData(char inData[], byte index) {
  // tokenize the input from the terminal by spaces
  char * words[MAX_TERMINAL_WORDS];
  byte current_word_index = 0;
  char * p = strtok(inData, " ");
  while(p != NULL) {
    words[current_word_index++] = p;
    p = strtok(NULL, " ");
  }

  if (strcmp(words[0], ":help") == 0) {
    printHelpText();

	} (strcmp(words[0], ":dm") == 0) {
		// direct message
	} (strcmp(words[0], ":name") == 0) {
		// change name
	} (strcmp(words[0], ":join") == 0) { 
		// join group chat
  } else 
    // checks if address field was given valid characters
    if ((strspn(words[1], "1234567890AaBbCcDdEeFf") <= 4)
        && (strspn(words[1], "1234567890AaBbCcDdEeFf") > 0)) {

      uint16_t TOaddr = strtol(words[1], NULL, 16);
      byte payload_id = random(255);

      if (strncmp(words[2], "-p", 2) == 0) { // Send ping
        Payload myPayload = {payload_id, PING, TOaddr, {'\0'}};

        radio.stopListening();
        radio.openWritingPipe(MULTI_ADDR);
        // CHANGED CODE
        sendPacket(&myPayload, sizeof(myPayload), true);
        Serial.print("Sending payload with id ");
        Serial.println(payload_id);
        radio.startListening();

      } else if (strcmp(words[2], "-l") == 0) { // Send LED pattern
        if (strspn(words[3], "1234567890") == 1) {
          byte led_patt = (byte) atoi(words[3]);
          Payload myPayload = {payload_id, LED, TOaddr, {led_patt}}; 

          radio.stopListening();
          radio.openWritingPipe(MULTI_ADDR);
          sendPacket(&myPayload, sizeof(myPayload), true);
          Serial.print("Sending payload with id ");
          Serial.println(payload_id);
          radio.startListening();
        }

        else {
          Serial.println("  Invalid LED pattern field.");
        }

      } else if (strcmp(words[2], "-m") == 0) { // Send message
        char str_msg[MAX_TERMINAL_MESSAGE_LEN];

        char * curr_pos = str_msg;
        for (int i = 3; i < current_word_index; i++){
          byte curr_len = strlen(words[i]);
          strncpy(curr_pos, words[i], curr_len);
          curr_pos += curr_len;

          // this will add in the space characters that tokenizing removes
          if (i < current_word_index - 1){
            strncpy(curr_pos, " ", 1);
            curr_pos++;
          }
        }

        Payload myPayload = {payload_id, MESS, TOaddr, {}};

        // the end of the string minus the start of the string gives the length
        memcpy(&myPayload.data, str_msg, curr_pos - str_msg);
        Serial.println(myPayload.data);
        radio.stopListening();
        radio.openWritingPipe(MULTI_ADDR);
        sendPacket(&myPayload, sizeof(myPayload), true);
        Serial.print("Sending payload with id ");
        Serial.println(payload_id);
        radio.startListening();
      }

      else {
        Serial.println("  Invalid command field.");
      }
    }

    else {
      Serial.println("  Invalid address field.");
    }

  } else if (strcmp(words[0], "channel") == 0) {

    // Set radio channel
    byte chn = (byte) atoi(words[1]);

    if (chn >= 0 && chn <= 83) {
      Serial.print("Channel is now set to ");
      Serial.println(chn);
      radio.setChannel(chn);
    } else {
      Serial.println(" Invalid channel number. Legal channels are 0 - 83.");
    }

  } else if (strcmp(words[0], "radio") == 0) {
    // Turn radio listening on/off
    if (strcmp(words[1], "on") == 0) {
      Serial.println("Radio is now in listening mode");
      radio.startListening();
    } else if (strcmp(words[1], "off") == 0) {
      Serial.println("Radio is now NOT in listening mode");
      radio.stopListening();
    } else {
      Serial.println(" Invalid syntax.");
    }
  } else if (strcmp(words[0], "multi") == 0) {
      byte payload_id = random(255);
      byte led_patt = (byte) atoi(words[1]);
      Payload myPayload = {payload_id, LED, MULTI_ADDR, {led_patt}};

      Serial.println("multicast");
      radio.stopListening();
      radio.openWritingPipe(MULTI_ADDR);
      sendPacket(&myPayload, sizeof(myPayload), true);
      #if DEBUG
        Serial.print("Sending payload with id ");
        Serial.print(payload_id);
      #endif
      radio.startListening();
  } else if(strcmp(words[0], "payload-cache") == 0) {
      printPayloadCache();
  }
}

void sendPacket(struct Payload *buf, uint8_t len, bool origin) {
  radio.multicastWrite(buf, len);
  if(origin) {
    addToPayloadCache(buf->payload_id);
  }
}

// Grab message received by nRF for this node
void handlePayload(struct Payload * myPayload) {
  #if DEBUG
    Serial.println("Handling payload...");
    Serial.print("Payload ID: ");
    Serial.println(myPayload->payload_id);
    Serial.print("Payload command: ");
    Serial.println(myPayload->command);
    Serial.print("Payload address: 0x");
    Serial.println(myPayload->address, HEX);
    Serial.print("Payload data: 0x");
    int i = 0;
    for (i=0; i<28; i++)
    {
    Serial.print(myPayload->data[i], HEX);
    }
    Serial.println("");
    
    printPayloadCache();
  #endif

  if(!isInCache(myPayload->payload_id, payload_cache, sizeof(payload_cache))) {
    // If we haven't seen this payload before...

    // Update last_payload
    #if DEBUG 
      Serial.println("Updating payload_cache...");
    #endif
    addToPayloadCache(myPayload->payload_id);

    // Propogate the payload
    if(myPayload->address != this_node_address) {
      #if DEBUG
        Serial.print("forwarding payload for address 0x");
        Serial.println(myPayload->address, HEX);
      #endif
      radio.stopListening();
      radio.openWritingPipe(MULTI_ADDR);
      sendPacket(myPayload, sizeof(Payload));
      #if DEBUG
        Serial.print("Sending payload with id ");
        Serial.println(myPayload->payload_id);
        Serial.print(" and command ");
        Serial.println(myPayload->command);
      #endif
      radio.startListening();
    }

    if(myPayload->address == MULTI_ADDR || myPayload->address == this_node_address) {
      // If this payload is for everybody, or us, act on it.
      
      #if DEBUG
        Serial.print("Payload accepted for address 0x");
        Serial.println(myPayload->address, HEX);
      #endif

      switch(myPayload->command) {
        case PING:
          Serial.println("Someone pinged us!");
          printPrompt();
          break;
    
        case LED:
          ledDisplay(myPayload->data[0]); // TODO: Make this non-blocking
          break;
    
        case MESS:
          Serial.print("Message:\r\n  ");
          Serial.println(myPayload->data);
          printPrompt();
          break;
    
        default:
          Serial.println("Invalid command received.");
          break;
      }
    }
  }
  
  free(myPayload); // Deallocate payload memory block
  #if DEBUG
    Serial.println("");
  #endif
}

bool isInCache(int value, int *cache, size_t size) {
  for(int i = 0; i < size; i++) {
    if(cache[i] == value) {
      return true;
    } 
  }
  return false;
}

uint8_t addToPayloadCache(byte payload_id) {
  payload_cache[payload_cache_index] = payload_id;
  payload_cache_index = payload_cache_index > PAYLOAD_CACHE_SIZE - 2 ? 0 : payload_cache_index + 1;
}

void printPrompt(void){
  Serial.print("> ");
}

/*
// Display LED pattern

// LED numbering:

           9
       8       10
     7           11
   6               12
  5                 13
   4               14
     3           15
       2       16
           1

shift register 1-8: AA
shift register 9-16: BB

setValue data in hex: 0xAABB
where AA in binary = 0b[8][7][6][5][4][3][2][1]
      BB in binary = 0b[16][15][14][13][12][11][10][9]

*/
void ledDisplay(byte pattern) {
  setValue(0x0000);
  digitalWrite(SROE_PIN, LOW);
  if(pattern == 0) {
    word pattern = 0x0000; // variable used in shifting process
    int del = 62; // ms value for delay between LED toggles

    for(int i=0; i<16; i++) {
      pattern = (pattern << 1) | 0x0001;
      setValue(pattern);
      delay(del);
    }

    for(int i=0; i<16; i++) {
      pattern = (pattern << 1);
      setValue(pattern);
      delay(del);
    }
  }
  else if(pattern == 1) {
    word pattern = 0x0000; // variable used in shifting process
    int del = 62; // ms value for delay between LED toggles

    for (int i = 0; i < 16; i++) {
      pattern = (pattern >> 1) | 0x8000;
      setValue(pattern);
      delay(del);
    }
    for (int i=0; i<16; i++) {
      pattern = (pattern >> 1);
      setValue(pattern);
      delay(del);
    }
  }
  else if(pattern == 2) {
    int del = 100;
    setValue(0x1010);
    delay(del);
    setValue(0x3838);
    delay(del);
    setValue(0x7C7C);
    delay(del);
    setValue(0xFEFE);
    delay(del);
    setValue(0xFFFF);
    delay(del);
    setValue(0xEFEF);
    delay(del);
    setValue(0xC7C7);
    delay(del);
    setValue(0x8383);
    delay(del);
    setValue(0x0101);
    delay(del);
    setValue(0x0000);
    delay(del);
  }
  else if(pattern == 3) {
    word pattern = 0x0101;
    int del = 125;
    setValue(pattern);
    for(int i=0; i<8; i++) {
      delay(del);
      pattern = (pattern << 1);
      setValue(pattern);
    }
  }
  else if(pattern == 4) {
    for (int i = 0; i < 4; i++) {
      setValue(0xFFFF);
      delay(125);
      setValue(0x0000);
      delay(125);
    }
  }
  else if(pattern == 5) {
    setValue(0xFFFF);
    Serial.println("LED ON!");
    delay(500);
  }
  digitalWrite(SROE_PIN, HIGH);
}


// LED display demo
void displayDemo() {
  digitalWrite(SROE_PIN, LOW);
  for (int i = 0; i < 100; i++) {
    setValue(0xAAAA);
    delay(125);
    setValue(0x5555);
    delay(125);
  }
  digitalWrite(SROE_PIN, HIGH);
}

// Sends word sized value to both SRs & latches output pins
void setValue(word value) {
  byte Hvalue = value >> 8;
  byte Lvalue = value & 0x00FF;
  SPI.transfer(Lvalue);
  SPI.transfer(Hvalue);
  digitalWrite(SRLATCH_PIN, HIGH);
  digitalWrite(SRLATCH_PIN, LOW);
}

// Prints 'help' command
void printHelpText() {
  Serial.println("Available commands:");
  Serial.println("  help          - displays commands list.");
  Serial.println();
  Serial.println("  send [addr] [command] [data] - send packets to other node.");
  Serial.println("      [addr]    - address of destination node, as a 4 digit hex value");
  Serial.println("      [command] - command field.");
  Serial.println("        -p - ping destination node.");
  Serial.println("        -l - send LED pattern to destination node.");
  Serial.println("           - [data] - LED pattern. Valid range: 0-255.");
  Serial.println("        -m - send message to destination node.");
  Serial.println("           - [data] - message to be sent. Max 26 characters.");
  Serial.println();
  Serial.println("  channel [val] - change channel of your node.");
  Serial.println("                - [val] - new channel. Valid range: 0-83.");
  Serial.println();
  Serial.println("  radio [on | off] - turn radio on or off");
}

void welcomeMessage(void) {
  char hex_addr[10];
  sprintf(hex_addr, "%04x", this_node_address);
  Serial.print("\r\nWelcome to the BoilerMake Hackathon Badge Network...\r\n\n");
  Serial.print("Your address: ");
  Serial.println(hex_addr);
  Serial.print("\nAll commands must be terminated with a carriage return.\r\n"
      "Type 'help' for a list of available commands.\r\n\n> ");
}

void printPayloadCache(void) {
  Serial.print("The payload cache is [");
  for(int i = 0; i < PAYLOAD_CACHE_SIZE; i++) {
    Serial.print(payload_cache[i]);
    if(i != (PAYLOAD_CACHE_SIZE - 1)) {
      Serial.print(", ");
    }
  }
  Serial.println("]");
}
