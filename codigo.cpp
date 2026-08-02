const int RAIN_ANALOG_PIN = A0;
const int RAIN_THRESHOLD  = 300;
const int RAIN_SAMPLES    = 10; // cuántas lecturas se promedian por cada chequeo

const int LIMIT_OPEN    = 4;
const int LIMIT_CLOSE   = 5;

const int IN1 = 8;
const int IN2 = 9;
const int ENA = 10;

const int MOTOR_SPEED_FULL = 200;
const int MOTOR_SPEED_SLOW = 90;
const unsigned long SLOWDOWN_AFTER_MS = 4000; // CALIBRAR: ~70-80% del tiempo total de recorrido

const unsigned long RAIN_CONFIRM_MS = 3000;
const unsigned long DRY_CONFIRM_MS  = 5000; // bajado de 30s a 5s, ahora que la lectura es más estable
const unsigned long MAX_MOVE_MS     = 8000;
const unsigned long SWITCH_DEBOUNCE_MS = 30; // bajado un poco para que sea más fácil sostener el clic

enum RoofState { OPEN, CLOSED, CLOSING, OPENING, ERROR_STATE };
RoofState state = OPEN;

unsigned long moveStartTime = 0;
unsigned long rainSince = 0;
unsigned long drySince  = 0;

unsigned long openLimitSince  = 0;
unsigned long closeLimitSince = 0;

void setup() {
  pinMode(RAIN_ANALOG_PIN, INPUT);
  pinMode(LIMIT_OPEN, INPUT_PULLUP);
  pinMode(LIMIT_CLOSE, INPUT_PULLUP);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  stopMotor();
  Serial.begin(9600);
  Serial.println("Sistema iniciado.");
}

// Toma varias lecturas seguidas y devuelve el promedio, para suavizar el ruido normal del sensor
int readRainSmoothed() {
  long total = 0;
  for (int i = 0; i < RAIN_SAMPLES; i++) {
    total += analogRead(RAIN_ANALOG_PIN);
    delay(2); // pequeña pausa entre lecturas, no afecta la reactividad del sistema
  }
  return total / RAIN_SAMPLES;
}

bool checkLimitStable(int pin, unsigned long &sinceVar) {
  bool pressed = (digitalRead(pin) == LOW);
  if (pressed) {
    if (sinceVar == 0) sinceVar = millis();
    return (millis() - sinceVar > SWITCH_DEBOUNCE_MS);
  } else {
    sinceVar = 0;
    return false;
  }
}

void loop() {
  int rainValue = readRainSmoothed();
  bool raining = (rainValue > RAIN_THRESHOLD);

  bool atOpenLimit  = checkLimitStable(LIMIT_OPEN, openLimitSince);
  bool atCloseLimit = checkLimitStable(LIMIT_CLOSE, closeLimitSince);

  // Descomenta para calibrar el umbral con el valor ya promediado:
  // Serial.println(rainValue);

  if (raining) { if (rainSince == 0) rainSince = millis(); drySince = 0; }
  else         { if (drySince  == 0) drySince  = millis(); rainSince = 0; }

  bool rainConfirmed = rainSince && (millis() - rainSince > RAIN_CONFIRM_MS);
  bool dryConfirmed  = drySince  && (millis() - drySince  > DRY_CONFIRM_MS);

  if (state == ERROR_STATE) {
    if (rainConfirmed) {
      state = CLOSING;
      moveStartTime = millis();
      Serial.println("Reintentando cerrar tras error...");
    } else if (dryConfirmed) {
      state = OPENING;
      moveStartTime = millis();
      Serial.println("Reintentando abrir tras error...");
    }
  } else {
    if (rainConfirmed && state != CLOSED && state != CLOSING) {
      state = CLOSING;
      moveStartTime = millis();
      Serial.println("Lluvia detectada -> cerrando techo");
    }
    if (dryConfirmed && state != OPEN && state != OPENING) {
      state = OPENING;
      moveStartTime = millis();
      Serial.println("Sin lluvia -> abriendo techo");
    }
  }

  switch (state) {
    case CLOSING:
      if (atCloseLimit) {
        stopMotor();
        state = CLOSED;
        Serial.println("Techo cerrado (switch activado)");
      } else if (millis() - moveStartTime > MAX_MOVE_MS) {
        stopMotor();
        state = ERROR_STATE;
        Serial.println("ERROR: tarda demasiado en cerrar, revisa el mecanismo o el switch");
      } else {
        driveMotor(false, currentSpeed());
      }
      break;

    case OPENING:
      if (atOpenLimit) {
        stopMotor();
        state = OPEN;
        Serial.println("Techo abierto (switch activado)");
      } else if (millis() - moveStartTime > MAX_MOVE_MS) {
        stopMotor();
        state = ERROR_STATE;
        Serial.println("ERROR: tarda demasiado en abrir, revisa el mecanismo o el switch");
      } else {
        driveMotor(true, currentSpeed());
      }
      break;

    default:
      stopMotor();
      break;
  }
}

int currentSpeed() {
  unsigned long elapsed = millis() - moveStartTime;
  if (elapsed > SLOWDOWN_AFTER_MS) {
    return MOTOR_SPEED_SLOW;
  }
  return MOTOR_SPEED_FULL;
}

void driveMotor(bool openDir, int speed) {
  digitalWrite(IN1, openDir ? HIGH : LOW);
  digitalWrite(IN2, openDir ? LOW  : HIGH);
  analogWrite(ENA, speed);
}

void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 0);
}