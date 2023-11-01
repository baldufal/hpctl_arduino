// the debounce time; increase if the output flickers
#define DEBOUNCE_DELAY 50

#define CURSOR_POS_LEVELS 8
#define HEATING_LEVELS 31
#define VENT_IN_LEVELS 7
#define VENT_OUT_LEVELS 5

// {Built in, Next, Prev, Plus, Minus}
static uint buttonPins[] = { 14, 10, 11, 12, 13 };

// the current reading from the buttons
static int buttonState[] = { LOW, LOW, LOW, LOW, LOW };
// the previous reading from the buttons
static int lastButtonState[] = { LOW, LOW, LOW, LOW, LOW };
// the last time the buttons were toggeled
static uint lastDebounceTime[] = { 0, 0, 0, 0, 0 };

static int cursorPos = 0;

void setupButtons() {
  for (size_t i = 0; i < 5; ++i) {
    pinMode(buttonPins[i], INPUT);
  }
}

void handleButtons() {
  for (size_t i = 0; i < 5; ++i) {

    int reading = digitalRead(buttonPins[i]);

    // If the switch changed, due to noise or pressing:
    if (reading != lastButtonState[i]) {
      // reset the debouncing timer
      lastDebounceTime[i] = millis();
    }

    if ((millis() - lastDebounceTime[i]) > DEBOUNCE_DELAY) {
      // whatever the reading is at, it's been there for longer than the debounce
      // delay, so take it as the actual current state:

      // if the button state has changed:
      if (reading != buttonState[i]) {
        buttonState[i] = reading;

        // Only take action if the new button state is HIGH
        if (buttonState[i] == HIGH) {
          switch (i) {
            // Built in
            case 0:
              cursorPos = (cursorPos + 1) % CURSOR_POS_LEVELS;
              break;
            // Next
            case 1:
              cursorPos = (cursorPos + 1) % CURSOR_POS_LEVELS;
              break;
            // Prev
            case 2:
              cursorPos -= 1;
              if (cursorPos < 0)
                cursorPos = CURSOR_POS_LEVELS - 1;
              break;
            // Plus
            case 3:
              switch (cursorPos) {
                case 0:
                  nextState.heating = (nextState.heating + 1) % HEATING_LEVELS;
                  stateChanged();
                  break;
                case 1:
                  nextState.vent_in = (nextState.vent_in + 1) % VENT_IN_LEVELS;
                  stateChanged();
                  break;
                case 2:
                  nextState.vent_out = (nextState.vent_out + 1) % VENT_OUT_LEVELS;
                  stateChanged();
                  break;
                default:
                  uint8_t mask = (uint8_t)1 << (cursorPos - 3);
                  nextState.ssrs = nextState.ssrs ^ mask;
                  stateChanged();
                  break;
              }
              break;
            // Minus
            case 4:
              switch (cursorPos) {
                case 0:
                  if (nextState.heating == 0)
                    nextState.heating = HEATING_LEVELS;
                  nextState.heating = (nextState.heating - 1) % HEATING_LEVELS;
                  stateChanged();
                  break;
                case 1:
                  if (nextState.vent_in == 0)
                    nextState.vent_in = VENT_IN_LEVELS;
                  nextState.vent_in = (nextState.vent_in - 1) % VENT_IN_LEVELS;
                  stateChanged();
                  break;
                case 2:
                  if (nextState.vent_out == 0)
                    nextState.vent_out = VENT_OUT_LEVELS;
                  nextState.vent_out = (nextState.vent_out - 1) % VENT_OUT_LEVELS;
                  stateChanged();
                  break;
                default:
                  uint8_t mask = (uint8_t)1 << (cursorPos - 3);
                  nextState.ssrs = currentState.ssrs ^ mask;
                  stateChanged();
                  break;
              }
              break;
          }
        }
      }
    }

    lastButtonState[i] = reading;
  }
}