# Techo tipo celosía automático activado por lluvia

Sistema basado en Arduino que cierra un techo plegable (celosía) cuando detecta lluvia y lo vuelve a abrir cuando el ambiente lleva un tiempo seco. La posición final (abierto/cerrado) se detecta con microswitches de fin de carrera, no por tiempo, para que el sistema no dependa de calibración y no se desajuste con el uso.

## Componentes usados

- Arduino UNO
- Sensor de lluvia (módulo FC-37 / YL-83, con placa colectora + placa comparadora)
- Driver de motor L298N (módulo dual H-bridge)
- Motorreductor DC (motor amarillo con caja reductora)
- 2 microswitches (fin de carrera): uno para la posición "abierto" y otro para "cerrado"
- Fuente de alimentación externa para el motor (batería de 6-12V, separada del Arduino)
- Cables jumper, protoboard (opcional)

## Diagrama de conexiones

### 1. Sensor de lluvia → Arduino

| Pin del sensor | Conectar a           |
|---|---|
| VCC | 5V del Arduino |
| GND | GND del Arduino |
| DO (salida digital) | D2 |
| AO (salida analógica) | sin conectar por ahora |

### 2. L298N → Arduino

| Pin del L298N | Conectar a |
|---|---|
| IN1 | D8 |
| IN2 | D9 |
| ENA | D10 (retira el jumper físico que trae puesto sobre ENA para poder controlar la velocidad por PWM) |
| GND (lado lógica) | GND del Arduino |

### 3. L298N → Motor y alimentación

| Terminal del L298N | Conectar a |
|---|---|
| OUT1 / OUT2 (salida "Motor A") | los 2 cables del motorreductor |
| Bloque de alimentación de motor (12V / GND) | batería externa 6-12V |
| GND de esa batería externa | unir también al GND del Arduino (tierra común, obligatorio) |

**Importante:** el Arduino se alimenta por su propio cable USB o fuente propia. Nunca alimentes el motor desde el pin 5V del Arduino — no da suficiente corriente y puede reiniciar la placa o dañarla.

### 4. Microswitches → Arduino

Cada microswitch tiene contactos COM, NA (normalmente abierto) y NC (normalmente cerrado). Usa COM + NC o COM + NA, según convenga a tu montaje mecánico; lo importante es que se presione exactamente cuando el mecanismo llegue al tope.

| Switch | Conectar a |
|---|---|
| Fin de carrera "abierto" | una pata a GND, la otra a D4 |
| Fin de carrera "cerrado" | una pata a GND, la otra a D5 |

No se necesitan resistencias externas: el código usa `INPUT_PULLUP`, así que el pin está en HIGH por defecto y cae a LOW cuando el switch conecta a GND.

**Posicionamiento físico:** monta cada switch en el extremo del recorrido de la celosía, de modo que una leva, brazo o el propio marco lo presione justo al llegar al tope — ni antes ni después, o el techo quedará mal cerrado/abierto.

## Diagrama de pines (resumen)

```
Arduino UNO
├── D2  → Sensor de lluvia (DO)
├── D4  → Microswitch "abierto"
├── D5  → Microswitch "cerrado"
├── D8  → L298N IN1
├── D9  → L298N IN2
├── D10 → L298N ENA
├── 5V  → VCC sensor de lluvia
└── GND → GND común (sensor, switches, L298N lógica, batería del motor)

L298N
├── OUT1/OUT2 → Motorreductor
└── Bloque 12V/GND → Batería externa 6-12V
```

## Código

```cpp
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
```

## Cómo funciona

El sistema es una máquina de estados con 4 posiciones posibles: `OPEN`, `CLOSED`, `CLOSING`, `OPENING`.

1. **Lectura continua:** en cada vuelta del `loop()`, el Arduino lee el sensor de lluvia y los dos microswitches.

2. **Confirmación con tiempo (anti-falsos-positivos):** el sistema no reacciona a la primera gota ni a la primera pausa de lluvia. Necesita:
   - 3 segundos continuos de lluvia detectada antes de decidir cerrar.
   - 30 segundos continuos sin lluvia antes de decidir abrir.

   Esto evita que el techo esté abriendo y cerrando constantemente ("flapping") por una gota aislada o un corte momentáneo de la lluvia. Los tiempos (`RAIN_CONFIRM_MS`, `DRY_CONFIRM_MS`) se pueden ajustar a tu gusto.

3. **Movimiento hacia el objetivo:** una vez confirmada la condición, el estado pasa a `CLOSING` u `OPENING` y el motor arranca en la dirección correspondiente (controlado por `IN1`/`IN2`, con velocidad fijada por PWM en `ENA`).

4. **Detección de llegada:** mientras se mueve, el programa revisa en cada ciclo si el microswitch correspondiente al destino ya se activó (`LOW`). En cuanto se activa, el motor se detiene inmediatamente y el estado pasa a `CLOSED` u `OPEN`. Así el punto de parada lo define el mecanismo físico, no un tiempo estimado — es más preciso y no se desajusta con el desgaste.

5. **Seguridad (`MAX_MOVE_MS`):** si el motor lleva más de 8 segundos moviéndose sin que el switch correspondiente se active, el programa asume que algo falló (mecanismo atascado, switch mal conectado o desalineado) y detiene el motor para no forzarlo ni quemarlo, imprimiendo un aviso por el monitor serial.

6. **Estado quieto:** cuando está en `OPEN` o `CLOSED`, el motor permanece apagado (`stopMotor()`) hasta que se confirme la condición contraria.

## Solución de problemas

| Síntoma | Posible causa / qué revisar |
|---|---|
| El motor no gira | LED de encendido del L298N apagado (falta alimentación externa), o falta la tierra común entre Arduino y la batería del motor |
| El motor gira siempre igual, no distingue dirección | Revisar que IN1/IN2 estén en los pines correctos según el código |
| El motor no se detiene al llegar al tope | El switch no se presiona en el momento correcto, o está en el pin equivocado. Prueba un sketch simple que imprima `digitalRead()` de cada switch mientras los presionas a mano |
| Se detiene antes de tiempo | Algo más está rozando el switch por accidente |
| Aparece "tardó demasiado" en el monitor serial | Motor sin fuerza, mecanismo atascado, o el switch nunca se activa |
| Se mueve al revés de lo esperado | Intercambiar los cables OUT1/OUT2 en el L298N, o invertir la lógica en `driveMotor()` |
| El Arduino se reinicia solo al arrancar el motor | Está compartiendo la misma fuente de alimentación que el motor; sepáralas y deja solo el GND en común |
| El sensor de lluvia siempre marca lluvia o siempre seco | Ajustar el potenciómetro de sensibilidad del módulo; probar mojando la placa colectora directamente y observar su LED indicador |
