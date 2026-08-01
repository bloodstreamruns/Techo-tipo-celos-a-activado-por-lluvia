// --- Techo tipo celosía activado por lluvia, con switches de fin de carrera ---
// Arduino UNO + Sensor de lluvia (FC-37/YL-83) + L298N + motorreductor DC
// + 2 microswitches (fin de carrera abierto / cerrado)

const int RAIN_DO_PIN   = 2;  // Salida digital del sensor de lluvia
const int LIMIT_OPEN    = 4;  // Switch de fin de carrera "abierto"
const int LIMIT_CLOSE   = 5;  // Switch de fin de carrera "cerrado"

const int IN1 = 8;   // L298N IN1
const int IN2 = 9;   // L298N IN2
const int ENA = 10;  // L298N ENA (PWM, velocidad)

const int MOTOR_SPEED = 200; // 0-255

const unsigned long RAIN_CONFIRM_MS = 3000;   // 3s de lluvia continua antes de cerrar
const unsigned long DRY_CONFIRM_MS  = 30000;  // 30s de sequedad antes de abrir
const unsigned long MAX_MOVE_MS     = 8000;   // tope de seguridad: si tarda más que esto, algo está mal

enum RoofState { OPEN, CLOSED, CLOSING, OPENING };
RoofState state = OPEN;

unsigned long moveStartTime = 0;
unsigned long rainSince = 0;
unsigned long drySince  = 0;

void setup() {
  pinMode(RAIN_DO_PIN, INPUT);
  pinMode(LIMIT_OPEN, INPUT_PULLUP);
  pinMode(LIMIT_CLOSE, INPUT_PULLUP);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  stopMotor();
  Serial.begin(9600);
  Serial.println("Sistema iniciado.");
}

void loop() {
  bool raining = (digitalRead(RAIN_DO_PIN) == LOW); // LOW = detecta agua
  bool atOpenLimit  = (digitalRead(LIMIT_OPEN)  == LOW); // LOW = switch presionado
  bool atCloseLimit = (digitalRead(LIMIT_CLOSE) == LOW);

  if (raining) { if (rainSince == 0) rainSince = millis(); drySince = 0; }
  else         { if (drySince  == 0) drySince  = millis(); rainSince = 0; }

  bool rainConfirmed = rainSince && (millis() - rainSince > RAIN_CONFIRM_MS);
  bool dryConfirmed  = drySince  && (millis() - drySince  > DRY_CONFIRM_MS);

  // Decidir si hay que empezar a moverse
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

  // Ejecutar el movimiento y revisar límites
  switch (state) {
    case CLOSING:
      if (atCloseLimit) {
        stopMotor();
        state = CLOSED;
        Serial.println("Techo cerrado (switch activado)");
      } else if (millis() - moveStartTime > MAX_MOVE_MS) {
        stopMotor();
        Serial.println("ERROR: tardó demasiado en cerrar, revisa el mecanismo o el switch");
      } else {
        driveMotor(false);
      }
      break;

    case OPENING:
      if (atOpenLimit) {
        stopMotor();
        state = OPEN;
        Serial.println("Techo abierto (switch activado)");
      } else if (millis() - moveStartTime > MAX_MOVE_MS) {
        stopMotor();
        Serial.println("ERROR: tardó demasiado en abrir, revisa el mecanismo o el switch");
      } else {
        driveMotor(true);
      }
      break;

    default:
      stopMotor();
      break;
  }
}

void driveMotor(bool openDir) {
  digitalWrite(IN1, openDir ? HIGH : LOW);
  digitalWrite(IN2, openDir ? LOW  : HIGH);
  analogWrite(ENA, MOTOR_SPEED);
}

void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 0);
}