enum read_status {
  PIN_X,
  PIN_Y,
  PIN_Z,
  PIN_4,
  PIN_X1,
  PIN_X10,
  PIN_X100,
};
int pinValues[7] = {
  2,
  3,
  4,
  5,
  6,
  7,
  8
};

int encoder0PinA = A1;
int encoder0PinB = A2;
int encoder0Pos = 0;
int encoder0PinALast = LOW;
int n = LOW;


void setup() {
  for(int i = 0; i < 7; i++){
    // Input Pullup, da die Pins sonst PullUp-Widerstände brauchen. Anderes Ende ist mit GND verbunden.
    pinMode(pinValues[i], INPUT_PULLUP);
  }
  pinMode(encoder0PinA,INPUT);
  pinMode(encoder0PinB,INPUT);
  Serial.println("Started up");
}

void readButtons(){
  for(int i = 0; i < 7; i++){
    int pin = pinValues[i];
    int value = digitalRead(pin);
    if(value == 0){
      switch(i){
        case PIN_X:
          Serial.println("Button PinX pressed");
          break;
        case PIN_Y:
          Serial.println("Button PinY pressed");
          break;
        case PIN_Z:
          Serial.println("Button PinZ pressed");
        break;
        case PIN_4:
          Serial.println("Button Pin4 pressed");
          break;
        case PIN_X1:
          Serial.println("Button PinX1 pressed");
          break;
        case PIN_X10:
          Serial.println("Button PinX10 pressed");
        break;
        case PIN_X100:
          Serial.println("Button PinX100 pressed");
          break;
      }
    }
  }
}
void printValueOfPin(int pin){
    int value = analogRead(pin);
    Serial.print("Pin");
    Serial.print(pin);
    Serial.print(" ");
    Serial.print(value);
    Serial.println();
    
}

void loop() {
  //Serial.println("Loop...");
  // Taken from https://playground.arduino.cc/Main/RotaryEncoders#Example1
  
  n = digitalRead(encoder0PinA);
  if ((encoder0PinALast == LOW) && (n == HIGH)) {
    if (digitalRead(encoder0PinB) == LOW) {
      encoder0Pos--;
    } else {
      encoder0Pos++;
    }
    Serial.print (encoder0Pos);
    Serial.print ("/");
  }
  encoder0PinALast = n;
     
  //delay(1000);

}
