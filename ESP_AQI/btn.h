// copyright (c) 2018 Sam C. Lin

#define BTN_PRESS_SHORT 50  // ms
#define BTN_PRESS_LONG 500 // ms

#define BTN_STATE_OFF   0
#define BTN_STATE_SHORT 1 // short press
#define BTN_STATE_LONG  2 // long press

class Btn {
  uint8_t btnGpio;
  uint8_t buttonState;
  unsigned long lastDebounceTime;  // the last time the output pin was toggled
  
public:
  Btn(uint8_t gpio);
  void init();

  void read();
  uint8_t shortPress();
  uint8_t longPress();
};

Btn::Btn(uint8_t gpio)
{
  btnGpio = gpio;
  buttonState = BTN_STATE_OFF;
  lastDebounceTime = 0;
}

void Btn::init()
{
  pinMode(btnGpio,INPUT_PULLUP);
}

void Btn::read()
{
  uint8_t sample;
  unsigned long delta;
  sample = digitalRead(btnGpio) ? 0 : 1;
  if (!sample && (buttonState == BTN_STATE_LONG) && !lastDebounceTime) {
    buttonState = BTN_STATE_OFF;
  }
  if ((buttonState == BTN_STATE_OFF) ||
      ((buttonState == BTN_STATE_SHORT) && lastDebounceTime)) {
    if (sample) {
      if (!lastDebounceTime && (buttonState == BTN_STATE_OFF)) {
	lastDebounceTime = millis();
      }
      delta = millis() - lastDebounceTime;

      if (buttonState == BTN_STATE_OFF) {
	if (delta >= BTN_PRESS_SHORT) {
	  buttonState = BTN_STATE_SHORT;
	}
      }
      else if (buttonState == BTN_STATE_SHORT) {
	if (delta >= BTN_PRESS_LONG) {
	  buttonState = BTN_STATE_LONG;
	}
      }
    }
    else { //!sample
      lastDebounceTime = 0;
    }
  }
}

uint8_t Btn::shortPress()
{
  if ((buttonState == BTN_STATE_SHORT) && !lastDebounceTime) {
    buttonState = BTN_STATE_OFF;
    return 1;
  }
  else {
    return 0;
  }
}

uint8_t Btn::longPress()
{
  if ((buttonState == BTN_STATE_LONG) && lastDebounceTime) {
    lastDebounceTime = 0;
    return 1;
  }
  else {
    return 0;
  }
}
