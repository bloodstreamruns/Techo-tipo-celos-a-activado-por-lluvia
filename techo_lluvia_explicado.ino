// ============================================================
// TECHO TIPO CELOSÍA (PERSIANA DE CORDÓN) ACTIVADO POR LLUVIA
// Versión final, explicada línea por línea para alguien que
// nunca ha programado un Arduino.
// ============================================================
//
// Conceptos generales que se repiten todo el código:
//
// - "Pin": cada patita numerada del Arduino (D2, D8, A0, etc.) por
//   donde se lee o se manda una señal eléctrica.
// - HIGH / LOW: los dos únicos valores de un pin digital
//   (5V / 0V, encendido / apagado).
// - Variable: una "cajita" con nombre donde se guarda un dato.
// - Función: un bloque de instrucciones con nombre, que se
//   "llama" escribiendo ese nombre seguido de paréntesis.
// - Todo lo que empieza con // es un comentario: el Arduino lo
//   ignora, es solo para que un humano lo entienda.

// --------------------------------------------------------------
// SECCIÓN 1: constantes — números fijos que nunca cambian
// --------------------------------------------------------------

const int RAIN_ANALOG_PIN = A0;
// El sensor de lluvia está conectado a un pin ANALÓGICO (A0), no
// digital. A diferencia de un pin digital (que solo entiende
// HIGH/LOW), un pin analógico puede leer valores intermedios, como
// un "volumen" del 0 al 1023. Esto es necesario porque el
// componente de sensor de lluvia que estamos usando solo da una
// señal de este tipo, no una de encendido/apagado limpia.

const int RAIN_THRESHOLD  = 300;
// El número (entre 0 y 1023) a partir del cual decidimos "sí está
// lloviendo". Hay que calibrarlo observando qué valor da el sensor
// normalmente en seco, y poniendo este número un poco por encima.

const int RAIN_SAMPLES    = 10;
// Cuántas veces vamos a leer el sensor SEGUIDAS antes de sacar un
// promedio. Leer una sola vez puede caer justo en un "pico" de
// ruido eléctrico pasajero; promediar varias lecturas suaviza eso.

const int LIMIT_OPEN    = 4;  // Interruptor de fin de carrera "abierto"
const int LIMIT_CLOSE   = 5;  // Interruptor de fin de carrera "cerrado"

const int IN1 = 8;   // Cable de control 1 hacia el L298N (dirección)
const int IN2 = 9;   // Cable de control 2 hacia el L298N (dirección)
const int ENA = 10;  // Cable de velocidad hacia el L298N (debe ser un pin "~", con PWM)

const int MOTOR_SPEED_FULL = 200;
// Velocidad "normal" del motor (escala 0-255), usada la mayor parte
// del recorrido.

const int MOTOR_SPEED_SLOW = 90;
// Velocidad reducida, usada cerca del final del recorrido, para que
// el motor no golpee bruscamente el tope y no fuerce los cordones.

const unsigned long SLOWDOWN_AFTER_MS = 4000;
// "unsigned long" es un tipo de número entero grande y siempre
// positivo — se usa para contar milisegundos porque esos números
// crecen rápido. Este valor dice: "después de 4 segundos moviéndose,
// cambia a velocidad reducida". Hay que calibrarlo cronometrando el
// tiempo real que tarda tu mecanismo, y usando un 70-80% de ese total.

const unsigned long RAIN_CONFIRM_MS = 3000;
// Cuánto tiempo (en milisegundos) tiene que estar lloviendo DE
// FORMA CONTINUA antes de decidir que sí hay que cerrar. 3000 = 3
// segundos. Evita reaccionar a un dato aislado.

const unsigned long DRY_CONFIRM_MS  = 5000;
// Lo mismo pero al revés: 5000 = 5 segundos secos antes de abrir.
// (Se bajó de 30s a 5s porque, al promediar la lectura del sensor
// con RAIN_SAMPLES, la señal ya es más estable por sí sola, y no
// hace falta esperar tanto para filtrar el ruido.)

const unsigned long MAX_MOVE_MS     = 8000;
// Límite de seguridad: si el motor lleva más de 8 segundos
// moviéndose sin que el interruptor de destino se active, algo
// salió mal (mecanismo atascado, cordón trabado, interruptor mal
// puesto) y hay que apagar el motor en vez de seguir forzándolo.

const unsigned long SWITCH_DEBOUNCE_MS = 30;
// Cuánto tiempo debe mantenerse presionado un interruptor, DE
// FORMA CONTINUA, antes de contarlo como válido. Esto filtra el
// "rebote" (bouncing): el fenómeno donde un switch mecánico (o
// incluso uno simulado con un clic) genera varias señales rápidas
// de encendido/apagado en vez de una sola señal limpia.

// --------------------------------------------------------------
// SECCIÓN 2: los estados posibles del sistema
// --------------------------------------------------------------

enum RoofState { OPEN, CLOSED, CLOSING, OPENING, ERROR_STATE };
// Un "enum" define un tipo de dato que solo puede valer una de las
// opciones listadas. El techo, en todo momento, está en uno de
// estos 5 estados:
//   OPEN        = completamente abierto y quieto
//   CLOSED      = completamente cerrado y quieto
//   CLOSING     = moviéndose hacia cerrado
//   OPENING     = moviéndose hacia abierto
//   ERROR_STATE = se detuvo por seguridad tras tardar demasiado;
//                 espera una nueva condición para reintentar

RoofState state = OPEN;
// Variable que guarda el estado actual. Empieza en OPEN, asumiendo
// que el techo arranca abierto quieto cuando se enciende el Arduino.

// --------------------------------------------------------------
// SECCIÓN 3: variables que sí cambian mientras el programa corre
// --------------------------------------------------------------

unsigned long moveStartTime = 0;
// En qué momento (según millis(), el "cronómetro" del Arduino)
// empezó el movimiento actual. Sirve para medir cuánto tiempo lleva
// moviéndose, y decidir cuándo bajar la velocidad o cuándo declarar
// un error por tardanza.

unsigned long rainSince = 0;
// Desde qué momento lleva lloviendo SIN INTERRUPCIÓN. 0 significa
// "no está lloviendo ahorita".

unsigned long drySince  = 0;
// Lo mismo pero para la sequedad.

unsigned long openLimitSince  = 0;
unsigned long closeLimitSince = 0;
// Estas dos se usan para el filtro anti-rebote de cada interruptor:
// guardan desde cuándo lleva presionado cada uno, sin soltar.

// ================================================================
// FUNCIÓN setup()
// Se ejecuta UNA SOLA VEZ, al encender el Arduino o darle reset.
// ================================================================
void setup() {

  pinMode(RAIN_ANALOG_PIN, INPUT);
  // Para pines analógicos, pinMode() no es estrictamente necesario,
  // pero ponerlo no causa ningún problema y deja claro en el código
  // para qué se usa este pin.

  pinMode(LIMIT_OPEN, INPUT_PULLUP);
  pinMode(LIMIT_CLOSE, INPUT_PULLUP);
  // INPUT_PULLUP dice: el Arduino deja este pin en HIGH por defecto
  // usando una resistencia que ya trae integrada, y el pin solo cae
  // a LOW cuando el interruptor lo conecta directamente a GND. Así
  // no necesitamos poner una resistencia física aparte.

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  // OUTPUT: estos pines los usamos para MANDAR una señal (hacia el
  // L298N), no para leer.

  stopMotor();
  // Nos aseguramos de que el motor arranque apagado, para que no se
  // mueva solo por accidente al encender el Arduino.

  Serial.begin(9600);
  // Activa la comunicación por el cable USB con la computadora, a
  // una velocidad de 9600 (tiene que coincidir con la que pongas en
  // el Monitor Serial al abrirlo).

  Serial.println("Sistema iniciado.");
  // Mensaje informativo para el humano, no afecta el funcionamiento.
}

// ================================================================
// FUNCIÓN readRainSmoothed()
// Lee el sensor varias veces seguidas y devuelve el PROMEDIO, para
// suavizar el ruido eléctrico normal de cualquier sensor analógico.
// ================================================================
int readRainSmoothed() {
  long total = 0;
  // "long" es un tipo de número entero que puede ser negativo o
  // positivo, y más grande que un "int" normal — lo usamos aquí
  // porque vamos a ir sumando varias lecturas y el total puede
  // crecer más de lo que un "int" garantiza soportar con seguridad.

  for (int i = 0; i < RAIN_SAMPLES; i++) {
    // Un "for" repite el bloque de código que sigue, un número
    // determinado de veces. Aquí se lee: "empieza con i en 0, repite
    // mientras i sea menor que RAIN_SAMPLES (10), y suma 1 a i en
    // cada vuelta." O sea, este bloque se ejecuta exactamente 10 veces.

    total += analogRead(RAIN_ANALOG_PIN);
    // analogRead() lee el pin analógico y devuelve un número de 0 a
    // 1023. "total += X" es una forma corta de escribir
    // "total = total + X" — vamos acumulando la suma de las 10 lecturas.

    delay(2);
    // Una pequeña pausa de 2 milisegundos entre cada lectura, para
    // dar tiempo a que el valor eléctrico se estabilice un poco entre
    // una lectura y la siguiente.
  }
  return total / RAIN_SAMPLES;
  // Dividimos la suma total entre el número de lecturas para obtener
  // el promedio, y lo devolvemos como resultado de la función.
}

// ================================================================
// FUNCIÓN checkLimitStable()
// Revisa si un interruptor de fin de carrera lleva presionado, DE
// FORMA CONTINUA, más tiempo del que dura el "rebote" típico.
// Recibe dos parámetros: qué pin revisar, y una variable (por
// referencia) donde guardar desde cuándo lleva presionado.
// ================================================================
bool checkLimitStable(int pin, unsigned long &sinceVar) {
  // El símbolo "&" antes de "sinceVar" significa que esta función
  // recibe la variable "por referencia": puede MODIFICAR la variable
  // original que le mandemos desde afuera, no solo leer una copia.
  // Esto es necesario porque la función necesita "recordar" el
  // momento en que empezó a estar presionado, de una llamada a la
  // siguiente.

  bool pressed = (digitalRead(pin) == LOW);
  // Con INPUT_PULLUP, "presionado" corresponde a LOW.

  if (pressed) {
    if (sinceVar == 0) sinceVar = millis();
    // Si está presionado ahora, y todavía no habíamos anotado desde
    // cuándo (sinceVar sigue en 0), anotamos el momento actual.
    // Este truco es lo que evita que sinceVar se actualice a cada
    // rato mientras sigue presionado, permitiendo medir "cuánto
    // tiempo lleva" presionado sin soltar.

    return (millis() - sinceVar > SWITCH_DEBOUNCE_MS);
    // Devolvemos true (activado de verdad) solo si ya pasó más
    // tiempo que SWITCH_DEBOUNCE_MS desde que empezó a presionarse.

  } else {
    sinceVar = 0;
    // Si NO está presionado, reiniciamos el contador a 0 — así, la
    // próxima vez que se presione, se vuelve a medir desde cero.
    return false;
  }
}

// ================================================================
// FUNCIÓN loop()
// Se repite sin parar, mientras el Arduino esté encendido. Aquí
// vive toda la lógica de decisión del sistema.
// ================================================================
void loop() {

  int rainValue = readRainSmoothed();
  // Llamamos a nuestra función de arriba para obtener el promedio
  // suavizado de la lectura del sensor, en vez de una lectura cruda.

  bool raining = (rainValue > RAIN_THRESHOLD);
  // "bool" es una variable de verdadero/falso. Aquí preguntamos:
  // "¿el promedio del sensor es mayor que el umbral?" Si sí,
  // "raining" queda en true.

  bool atOpenLimit  = checkLimitStable(LIMIT_OPEN, openLimitSince);
  bool atCloseLimit = checkLimitStable(LIMIT_CLOSE, closeLimitSince);
  // Usamos la función de filtro anti-rebote para cada interruptor,
  // en vez de leerlos directo con digitalRead().

  // Descomenta esta línea mientras calibras RAIN_THRESHOLD:
  // Serial.println(rainValue);

  // --- Medir cuánto tiempo lleva lloviendo o seco sin parar ---

  if (raining) { if (rainSince == 0) rainSince = millis(); drySince = 0; }
  else         { if (drySince  == 0) drySince  = millis(); rainSince = 0; }
  // Misma lógica que ya vimos: si está lloviendo ahora, anota desde
  // cuándo (si no se había anotado ya) y borra el cronómetro de
  // sequedad; y viceversa.

  bool rainConfirmed = rainSince && (millis() - rainSince > RAIN_CONFIRM_MS);
  bool dryConfirmed  = drySince  && (millis() - drySince  > DRY_CONFIRM_MS);
  // "rainSince &&" al inicio: en programación, cualquier número
  // distinto de 0 se trata como "verdadero". Esto asegura que solo
  // calculemos la resta de tiempos si SÍ está lloviendo ahora mismo
  // (rainSince no es 0).

  // --- Decidir qué hacer según el estado actual ---

  if (state == ERROR_STATE) {
    // Si el sistema está "pausado por error", solo puede salir de
    // aquí si se confirma de nuevo alguna condición (lluvia o
    // sequedad) — así el sistema se auto-recupera sin quedar inútil
    // para siempre tras un fallo temporal.

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
    // Comportamiento normal (no estamos en estado de error):

    if (rainConfirmed && state != CLOSED && state != CLOSING) {
      // "&&" significa "Y" (ambas condiciones deben cumplirse).
      // "!=" significa "diferente de". Esto dice: "si ya se confirmó
      // lluvia, Y el techo no está ya cerrado, Y tampoco ya se está
      // cerrando, entonces hay que empezar a cerrarlo ahora."
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

  // --- Ejecutar el movimiento según el estado, y revisar límites ---

  switch (state) {
  // "switch" ejecuta un bloque distinto según el valor de "state",
  // más ordenado que escribir muchos "if" seguidos.

    case CLOSING:
      if (atCloseLimit) {
        // Si el interruptor de "cerrado" ya se activó de forma
        // estable...
        stopMotor();
        state = CLOSED;
        Serial.println("Techo cerrado (switch activado)");

      } else if (millis() - moveStartTime > MAX_MOVE_MS) {
        // Si no ha llegado, pero ya pasó demasiado tiempo moviéndose...
        stopMotor();
        state = ERROR_STATE;
        // A diferencia de una versión anterior de este código, aquí
        // SÍ cambiamos el estado a ERROR_STATE. Esto evita que el
        // programa siga imprimiendo el mensaje de error en cada
        // vuelta del loop() para siempre — ahora solo se imprime una
        // vez, y el sistema queda esperando una nueva condición.
        Serial.println("ERROR: tardó demasiado en cerrar, revisa el mecanismo o el switch");

      } else {
        // Si no ha llegado y tampoco se ha tardado demasiado, seguimos
        // moviendo el motor, con la velocidad que corresponda:
        driveMotor(false, currentSpeed());
      }
      break;
      // "break" corta la ejecución del switch aquí.

    case OPENING:
      // Misma lógica que CLOSING, pero en la dirección contraria.
      if (atOpenLimit) {
        stopMotor();
        state = OPEN;
        Serial.println("Techo abierto (switch activado)");
      } else if (millis() - moveStartTime > MAX_MOVE_MS) {
        stopMotor();
        state = ERROR_STATE;
        Serial.println("ERROR: tardó demasiado en abrir, revisa el mecanismo o el switch");
      } else {
        driveMotor(true, currentSpeed());
      }
      break;

    default:
      // Este caso cubre OPEN, CLOSED, y ERROR_STATE: en cualquiera de
      // estos, el motor debe estar apagado, sin ninguna razón para
      // moverse hasta que algo cambie el estado.
      stopMotor();
      break;
  }
}

// ================================================================
// FUNCIÓN currentSpeed()
// Decide qué velocidad usar según cuánto tiempo lleva el movimiento
// actual: normal al principio, reducida cerca del final.
// ================================================================
int currentSpeed() {
  unsigned long elapsed = millis() - moveStartTime;
  // Cuánto tiempo (en milisegundos) ha pasado desde que arrancó
  // este movimiento en particular.

  if (elapsed > SLOWDOWN_AFTER_MS) {
    return MOTOR_SPEED_SLOW;
    // Si ya pasó el tiempo de referencia, usamos la velocidad lenta
    // (para frenar suave cerca del tope).
  }
  return MOTOR_SPEED_FULL;
  // Si no, usamos la velocidad normal.
}

// ================================================================
// FUNCIÓN driveMotor()
// Mueve el motor en una dirección, a una velocidad dada.
// ================================================================
void driveMotor(bool openDir, int speed) {
  // "bool openDir": true = queremos abrir, false = queremos cerrar.
  // "int speed": la velocidad a usar (0-255), calculada afuera con
  // currentSpeed() y pasada aquí como parámetro.

  digitalWrite(IN1, openDir ? HIGH : LOW);
  // "openDir ? HIGH : LOW" es una forma corta de escribir un
  // if/else en una sola línea: "si openDir es true, usa HIGH; si es
  // false, usa LOW".

  digitalWrite(IN2, openDir ? LOW  : HIGH);
  // IN2 siempre recibe la señal CONTRARIA a IN1 — así es como el
  // L298N decide en qué sentido gira el motor.

  analogWrite(ENA, speed);
  // analogWrite() manda una señal PWM (una especie de
  // "encendido/apagado muy rápido") para simular un voltaje
  // intermedio, y así controlar la velocidad real del motor.
}

// ================================================================
// FUNCIÓN stopMotor()
// Apaga el motor por completo.
// ================================================================
void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  // Sin una combinación válida de HIGH/LOW en las dos direcciones,
  // el motor no tiene "orden" de girar hacia ningún lado.

  analogWrite(ENA, 0);
  // Y ponemos la velocidad en 0 también, como doble seguridad.
}
