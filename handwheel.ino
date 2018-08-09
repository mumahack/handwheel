/*
  Handwheel

  Reads an connected control unit (pendant) for a CNC machine and convert the measured signals to keystrokes, send via USB to the main host controller.
  The used pendant has two single pole multi-way selector switches (one to select an axises and one as step multiplier).
  Additional peripherals can be added and handled by extending the variable "controlsMap".
  
  
  created 28 July 2018
  by Robert Römer


  Additional details:

  Linux CNC keyboard control details
  http://www.linuxcnc.org/docs/2.4/html/gui_axis.html#cap:Most-Common-Keyboard

  More keyboard command details (keystrokes)
  https://www.arduino.cc/en/Reference/KeyboardModifiers

  TODO: check Keyboard.write to output correct axis commands (eg: KEY_LEFT_ARROW get send as P)
*/

#include <HID-Project.h>  // HID-Project by NicoHood, via Library Manager (alternate source https://github.com/NicoHood/HID)


// default value to be used for temporary keystroke variables and detection if a keystroke should be ignored
#define IGNORE_KEY '\0'

////
//  Definition of custom types for the following variables and control structures
////

enum TYPE {
  TYPE_AXIS,
  TYPE_STEP_MULTIPLIER,
  TYPE_OTHER,
};

enum AXIS {
  AXIS_OFF,
  AXIS_X,
  AXIS_Y,
  AXIS_Z,
  AXIS_4
};

// structure to define relation between controller wiring and how to react
struct PinDefinition {
  byte pin;        // the used controller input pin
  byte keystroke;  // keystroke to be send if a signal got detected
  String comment;  // comment for debugging
  TYPE type;       // type of control bound to this pin
  byte value;      // value to be used for the control type
  uint16_t lastDebounceMillis;  // used for debounce handling of the controller signal
};

// structure to define how which keystroke should be used for a axis position increment or decrement
struct AxisKeystrokes {
  AXIS axis;
  char down;
  char up;
};



////
//  Definition of the controller wiring and how to react
////


// available axis amount and keystrokes for jog increment and decrement
#define AXISMAP_SIZE 5
AxisKeystrokes axisMap[AXISMAP_SIZE] {
  // ATTENTION, order need to be identical to the AXIS struct
  { AXIS_OFF,  IGNORE_KEY,     IGNORE_KEY      },
  { AXIS_X,    KEY_LEFT_ARROW, KEY_RIGHT_ARROW },
  { AXIS_Y,    KEY_DOWN_ARROW, KEY_UP_ARROW    },
  { AXIS_Z,    KEY_PAGE_DOWN,  KEY_PAGE_UP     },
  { AXIS_4,    '[',            ']'             }
};


// map of connected controls and they definitions
#define CONTROLSMAP_SIZE 7
PinDefinition controlsMap[CONTROLSMAP_SIZE] = {
  {2,   'X', "axis X",  TYPE_AXIS,            axisMap[1].axis},  // Activate first axis
  {3,   'Y', "axis Y",  TYPE_AXIS,            axisMap[2].axis},  // Activate second axis
  {4,   'Z', "axis Z",  TYPE_AXIS,            axisMap[3].axis},  // Activate third axis
  {5,   '4', "axis 4",  TYPE_AXIS,            axisMap[4].axis},  // Activate fourth axis
  {6,   'a', "X1",      TYPE_STEP_MULTIPLIER, 1},
  {7,   'b', "X10",     TYPE_STEP_MULTIPLIER, 10},
  {8,   'c', "X100",    TYPE_STEP_MULTIPLIER, 100},
  // extend if more controls are connected, examples:
  /*
  {9,   'P', "Pause",   TYPE_OTHER},
  {10,  'S', "Resume",  TYPE_OTHER},
  {11,  KEY_ESC, "Stop",TYPE_OTHER},
  */
};


// default values for the axis and step multiplier
byte axis = AXIS_OFF;
unsigned int stepMultiplier = 1;

// define if keystroke for axis and step multiplier should be send only when its value change
bool axisKeystrokeOnlyOnUpdate = true;
bool multiplierKeystrokeOnlyOnUpdate = true;

// software debounce for above controller input readouts and reactions
unsigned int buttonDebounceDelay = 100;



//
// jog wheel encoder
//
int encoder0PinA = A1;
int encoder0PinB = A2;

// helper variables for the jog wheel
bool encoder0PinALast = LOW;
bool encoder0PinANow = LOW;



//
// uncomment to switch from keystrokes to serial output for debugging
//
#define DEBUG


//
// uncomment to switch from keystrokes to serial output for debugging
//
#define MULTIPLY_STEP_KEYSTROKE true


void setup() {
  // setup all control pins as input

  for (int i = 0; i < CONTROLSMAP_SIZE; i++) {
    pinMode(controlsMap[i].pin, INPUT_PULLUP);  // activate internal builtin pullup resistor, other end is connected to GND
  }
  pinMode(encoder0PinA, INPUT);
  pinMode(encoder0PinB, INPUT);

  // short delay to settle connections
  delay(500);

  #ifdef DEBUG
    // Start Serial as debugging interface
    Serial.begin(115200);
    while (!Serial) {}
    Serial.println("Handwheel DEBUG mode started");

    Serial.println();
    Serial.println("Controls config:");
    Serial.print("Pin");
    Serial.print("\t| ");
    Serial.print("Keystroke");
    Serial.print("\t| ");
    Serial.print("Type");
    Serial.print("\t| ");
    Serial.print("Value");
    Serial.print("\t| ");
    Serial.print("Comment");
    Serial.println();
    for ( unsigned int i = 0; i < CONTROLSMAP_SIZE; i++ ) {
      Serial.print(controlsMap[i].pin);
      Serial.print("\t| ");
      Serial.write(controlsMap[i].keystroke);
      Serial.print("\t\t| ");
      Serial.print(controlsMap[i].type);
      Serial.print("\t| ");
      Serial.print(controlsMap[i].value);
      Serial.print("\t| ");
      Serial.print(controlsMap[i].comment);
      Serial.println();
    }
    Serial.println();

    Serial.println();
    Serial.println("Axis jog config:");
    Serial.println("axis\t| Keystroke down | keystroke up\t");
    for ( unsigned int i = 0; i < AXISMAP_SIZE; i++ ) {
      Serial.print(axisMap[i].axis);
      Serial.print("\t| ");
      Serial.write(axisMap[i].down);
      Serial.print("\t\t | ");
      Serial.write(axisMap[i].up);
      Serial.println();
    }
    Serial.println();
  #else
    // Start Arduino keyboard
    Keyboard.begin();
  #endif
}

void handleControls() {
  #ifdef DEBUG
  bool headerShown = false;
  #endif

  uint16_t curMillis = millis();
  bool axisDetected = false;
  bool anyControlDetected = false;

  // iterate thru all control pins and check what's todo
  for ( int i = 0; i < CONTROLSMAP_SIZE; i++ ) {
    // check to debounce signal reflections and switching noise/bounce
    if ( (curMillis - controlsMap[i].lastDebounceMillis) < buttonDebounceDelay ) {
      continue;
    }
    // save time when the debounce check was passed the last time
    controlsMap[i].lastDebounceMillis = curMillis;

    int pin = controlsMap[i].pin;
    int state = !digitalRead(pin);  // invert read result, active signal is LOW
    byte keystroke = IGNORE_KEY;

    if ( state == 1 ) {
      anyControlDetected = true;
      switch ( controlsMap[i].type ) {
        case TYPE_STEP_MULTIPLIER:
          if ( multiplierKeystrokeOnlyOnUpdate == false || stepMultiplier != controlsMap[i].value ) {
            stepMultiplier = controlsMap[i].value;
            keystroke = controlsMap[i].keystroke;
          }
        break;
        case TYPE_AXIS:
          axisDetected = true;
          if ( axisKeystrokeOnlyOnUpdate == false || axis != controlsMap[i].value ) {
            axis = controlsMap[i].value;
            keystroke = controlsMap[i].keystroke;
          }
        break;
        default:
          keystroke = controlsMap[i].keystroke;
        break;
      }

      #ifdef DEBUG
        if ( headerShown == false ) {
          Serial.print("Pin");
          Serial.print("\t| ");
          Serial.print("Key Stroke");
          Serial.print("\t| ");
          Serial.print("Comment");
          Serial.println();
          headerShown = true;
        }
        Serial.print(pin);
        Serial.print("\t| ");
        if ( keystroke != IGNORE_KEY ) {
          Serial.write(keystroke);
          Serial.print("\t");
        } else {
          Serial.print("- skipped -");
        }
        Serial.print("\t| ");
        Serial.println(controlsMap[i].comment);
      #else
        // check if a control command could be identified and if a keystroke should be send
        if ( keystroke != 0 ) {
          Keyboard.write(keystroke);
        }
      #endif
    }
  }

  if ( anyControlDetected == true && axisDetected != true ) {
    // set axis to AXIS_OFF if incomming control signals got detected but nothing for the axis
    axis = AXIS_OFF;
    #ifdef DEBUG
      Serial.println("No selected axis detected. Deactivate axis jog control.");
    #endif
  }
}

void handleEncoder0() {
  encoder0PinANow = digitalRead(encoder0PinA);
  if ( (encoder0PinALast == LOW) && (encoder0PinANow == HIGH) ) {
    byte keystroke = digitalRead(encoder0PinB) == HIGH ? axisMap[axis].up : axisMap[axis].down;

    if ( keystroke != IGNORE_KEY ) {
      // send out keystroke
      #ifdef DEBUG
        Serial.write(keystroke);
        Serial.println("");
      #else
        Keyboard.write(keystroke);
      #endif

      // check if the keystroke should be send out multiplied
      #ifdef MULTIPLY_STEP_KEYSTROKE
        // repeat the sendout based on the current multiplier minus one
        for ( unsigned int i = stepMultiplier-1; i >= 1; i-- ) {
          #ifdef DEBUG
            Serial.write(keystroke);
            Serial.println(" (multiplied)");
          #else
            Keyboard.write(keystroke);
          #endif
        }
      #endif
    }
  }
}

void loop() {
  handleControls();
  handleEncoder0();
}
