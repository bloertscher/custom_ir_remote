/*
 * custom_ir_remote.ino

 Since my old AV receiver remote doesn't support some of my newer hardware, I'll use the Arduino
 as a translator.
 */

#include <Arduino.h>

/* Specify protocols for decoding */
// NEC for the Onkyo receiver and DVD player commands
#define DECODE_NEC

// Samsung covers the TV and the LG blu-ray player
#define DECODE_SAMSUNG

// Simple pin definitions for the ATmega328 boards like mine, the Uno
#define IR_RECEIVE_PIN 2  // To be compatible with interrupt example, pin 2 is chosen here.
#define IR_SEND_PIN 3
#define TONE_PIN 4
#define APPLICATION_PIN 5
#define ALTERNATIVE_IR_FEEDBACK_LED_PIN 6  // E.g. used for examples which use LED_BUILDIN for example output.
#define _IR_TIMING_TEST_PIN 7

/*
 * Helper macro for getting a macro definition as string
 */
#if !defined(STR_HELPER)
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#endif

// The main library. Make sure the pin definitions happen before this I think.
#include <IRremote.hpp>

const int SEND_BUTTON_PIN = APPLICATION_PIN;
const uint16_t TRANSLATION_ERROR = 0xffff;

unsigned long lastButtonPress = 0;

void setup() {
  Serial.begin(115200);
  // Just to know which program is running on my Arduino
  Serial.println(F("START " __FILE__ " from " __DATE__ "\r\nUsing IRremote library version " VERSION_IRREMOTE));

  // Use this pin to send a signal
  pinMode(SEND_BUTTON_PIN, INPUT_PULLUP);

  // Start the receiver and if not 3. parameter specified, take LED_BUILTIN pin from the internal boards definition as default feedback LED
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  IrSender.begin();

  Serial.print(F("Ready to receive IR signals of protocols: "));
  printActiveIRProtocols(&Serial);
  Serial.println(F("at pin " STR(IR_RECEIVE_PIN)));
}

void loop() {
  /*
  * Check if received data is available and if yes, try to decode it.
  * Decoded result is in the IrReceiver.decodedIRData structure.
  *
  * E.g. command is in IrReceiver.decodedIRData.command
  * address is in command is in IrReceiver.decodedIRData.address
  * and up to 32 bit raw data in IrReceiver.decodedIRData.decodedRawData
  */
  if (IrReceiver.decode()) {

    /*
    * Print a short summary of received data
    */
    IrReceiver.printIRResultShort(&Serial);
    IrReceiver.printIRSendUsage(&Serial);
    if (IrReceiver.decodedIRData.protocol == UNKNOWN) {
      Serial.println(F("Received noise or an unknown (or not yet enabled) protocol"));
      // We have an unknown protocol here, print more info
      IrReceiver.printIRResultRawFormatted(&Serial, true);
    }


    // If the command was intended for the DVD player, translate it from the dummy Onkyo DVD code to the Samsung/LG Blu ray player code
    static const uint16_t dummy_onkyo_dvd = 0x6cd2;
    static const uint16_t my_lg_blu_ray_address = 0x2d2d;
    static const int n_repeats = 3;

    if (IrReceiver.decodedIRData.address == dummy_onkyo_dvd) {
      const uint16_t tx_command = translate_DVD_command(IrReceiver.decodedIRData.command);

      if (tx_command != TRANSLATION_ERROR) {
        Serial.print(F("Sending command to blu ray player: "));
        Serial.print(my_lg_blu_ray_address, HEX);
        Serial.print(" ");
        Serial.println(tx_command, HEX);
        Serial.flush();

        IrReceiver.stop();
        IrSender.sendSamsung(my_lg_blu_ray_address, tx_command, n_repeats);
        delay(100);
        IrReceiver.start();
      }
    }

    Serial.println();

    /*
    * !!!Important!!! Enable receiving of the next value,
    * since receiving has stopped after the end of the current received data packet.
    */
    IrReceiver.resume();  // Enable receiving of the next value
  }

  // Check button for manually sending a command
  if ((digitalRead(SEND_BUTTON_PIN) == LOW) && (millis() - lastButtonPress > 1000)) {
    lastButtonPress = millis();
    Serial.println(F("Sending pause command"));
    Serial.flush();
    IrReceiver.stop();
    // force send a pause command
    IrSender.sendSamsung(0x2d2d, 0x38, 0);
    IrReceiver.start();
  }
}

uint16_t translate_DVD_command(const uint16_t received_command) {
  uint16_t tx_command = 0;
      switch (received_command) {
        case 0x8d:  // play
          tx_command = 0x31;
          break;
        case 0x93:  // pause
          tx_command = 0x38;
          break;
        case 0x8e:  // stop
          tx_command = 0x39;
          break;
        case 0x91:  // fast forward
          tx_command = 0x33;
          break;
        case 0x92:  // rewind
          tx_command = 0x32;
          break;
        case 0x8f:  // skip forward
          tx_command = 0x34;
          break;
        case 0x90:  // skip back
          tx_command = 0x35;
          break;
        default:
          tx_command = TRANSLATION_ERROR;
          Serial.print(F("Unsupported DVD player command: "));
          Serial.println(received_command, HEX);
          break;
      }

    return tx_command;
}
