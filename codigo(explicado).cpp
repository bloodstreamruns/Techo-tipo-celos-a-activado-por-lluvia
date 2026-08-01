// ============================================================
// TECHO TIPO CELOSÍA ACTIVADO POR LLUVIA
// Explicación línea por línea para quien nunca ha programado
// ============================================================
//
// Antes de empezar, unos conceptos generales que se usan TODO el rato:
//
// - Un "pin" es cada una de las patitas numeradas del Arduino (D2, D8, etc.)
//   por donde el Arduino puede leer señales de sensores o mandar señales
//   para encender/apagar cosas.
//
// - HIGH y LOW son los dos únicos "valores" que puede tener un pin digital:
//   HIGH = 5 voltios (encendido/verdadero), LOW = 0 voltios (apagado/falso).
//
// - Una variable es como una cajita con nombre donde guardamos un dato
//   (un número, un texto, un verdadero/falso) para usarlo más adelante.
//
// - Una función es un bloque de instrucciones al que le pones un nombre,
//   para no tener que reescribir el mismo código muchas veces. Se "llama"
//   escribiendo su nombre seguido de paréntesis, ej: stopMotor();
//
// - Todo lo que empieza con // es un comentario: el Arduino lo ignora
//   por completo, es solo texto para que un humano entienda el código.

// --------------------------------------------------------------
// SECCIÓN 1: "Constantes" — números que no van a cambiar nunca
// mientras el programa corre, y les damos un nombre para que el
// código se entienda (es más claro leer "RAIN_DO_PIN" que un "2"
// suelto sin contexto).
// --------------------------------------------------------------

const int RAIN_DO_PIN   = 2;  // El sensor de lluvia está conectado al pin digital 2
const int LIMIT_OPEN    = 4;  // El switch que avisa "ya llegué a la posición ABIERTA" está en el pin 4
const int LIMIT_CLOSE   = 5;  // El switch que avisa "ya llegué a la posición CERRADA" está en el pin 5

const int IN1 = 8;   // Cable de control 1 hacia el L298N (le dice al motor hacia dónde girar)
const int IN2 = 9;   // Cable de control 2 hacia el L298N (junto con IN1 define la dirección)
const int ENA = 10;  // Cable que controla la VELOCIDAD del motor (tiene que ser un pin especial "~PWM")

const int MOTOR_SPEED = 200;
// La velocidad del motor se maneja con un número del 0 al 255:
// 0 = motor apagado, 255 = velocidad máxima. Usamos 200 como punto intermedio-alto.

const unsigned long RAIN_CONFIRM_MS = 3000;
// "unsigned long" es un tipo de número entero que solo acepta positivos y puede
// ser muy grande — se usa así porque vamos a medir milisegundos (1000 ms = 1 segundo)
// y esos números crecen rápido. 3000 ms = 3 segundos.
// Esto significa: "hay que detectar lluvia de forma CONTINUA durante 3 segundos
// antes de decidir que sí está lloviendo de verdad" (para no reaccionar a una
// gota aislada).

const unsigned long DRY_CONFIRM_MS  = 30000;
// 30000 ms = 30 segundos. Igual que arriba pero al revés: hay que estar seco
// 30 segundos seguidos antes de decidir abrir el techo otra vez.

const unsigned long MAX_MOVE_MS     = 8000;
// 8000 ms = 8 segundos. Es un límite de seguridad: si el motor lleva más de
// 8 segundos moviéndose y el switch de destino todavía no se activó, algo
// salió mal (el mecanismo se atascó, un cable se soltó, etc.) y hay que
// apagar el motor para no forzarlo ni quemarlo.

// --------------------------------------------------------------
// SECCIÓN 2: definimos los "estados" posibles del sistema
// --------------------------------------------------------------

enum RoofState { OPEN, CLOSED, CLOSING, OPENING };
// Un "enum" (enumeración) es una forma de crear un tipo de dato nuevo que
// solo puede valer una de las opciones que listamos. Aquí decimos: el techo,
// en todo momento, solo puede estar en uno de estos 4 estados:
//   OPEN     = completamente abierto y quieto
//   CLOSED   = completamente cerrado y quieto
//   CLOSING  = en movimiento, yendo hacia cerrado
//   OPENING  = en movimiento, yendo hacia abierto
// Es como una lista de opciones de un menú, pero para el programa.

RoofState state = OPEN;
// Creamos una variable llamada "state" (estado) de tipo RoofState (el que
// acabamos de definir arriba), y le damos el valor inicial OPEN.
// O sea: cuando el Arduino se enciende, asumimos que el techo empieza abierto.

// --------------------------------------------------------------
// SECCIÓN 3: variables que sí van a cambiar mientras el programa corre
// (por eso NO llevan "const" — const es solo para lo que nunca cambia)
// --------------------------------------------------------------

unsigned long moveStartTime = 0;
// Aquí guardaremos EN QUÉ MOMENTO empezó el movimiento actual (abriendo o
// cerrando), para poder calcular después "cuánto tiempo lleva moviéndose".

unsigned long rainSince = 0;
// Aquí guardaremos desde qué momento empezó a detectarse lluvia SIN
// INTERRUPCIÓN. Si vale 0, quiere decir "no está lloviendo ahorita".

unsigned long drySince  = 0;
// Lo mismo pero para la sequedad: desde qué momento lleva seco sin parar.

// ================================================================
// FUNCIÓN setup()
// Esta función se ejecuta UNA SOLA VEZ, apenas el Arduino se enciende
// o se le da reset. Aquí se configuran cosas de arranque.
// ================================================================
void setup() {

  pinMode(RAIN_DO_PIN, INPUT);
  // pinMode() le dice al Arduino "para qué vas a usar este pin".
  // INPUT significa: este pin va a LEER una señal que le llega desde afuera
  // (en este caso, del sensor de lluvia).

  pinMode(LIMIT_OPEN, INPUT_PULLUP);
  pinMode(LIMIT_CLOSE, INPUT_PULLUP);
  // INPUT_PULLUP es un tipo especial de entrada: el Arduino deja el pin en
  // HIGH (5V) por defecto usando una resistencia interna, y el pin solo pasa
  // a LOW cuando el switch conecta ese pin directamente a GND (tierra).
  // Ventaja: no necesitamos poner una resistencia física aparte para que
  // el switch funcione bien; el Arduino ya trae una "por dentro".

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  // OUTPUT significa: este pin lo vamos a usar para MANDAR una señal hacia
  // afuera (en este caso, hacia el L298N, para controlar el motor).

  stopMotor();
  // Llamamos a la función stopMotor() (definida más abajo) para asegurarnos
  // de que el motor arranque APAGADO, y no se mueva solo por accidente
  // apenas se enciende el Arduino.

  Serial.begin(9600);
  // Esto activa la comunicación serial: permite que el Arduino "hable" con
  // la computadora a través del cable USB, para que podamos ver mensajes
  // de texto en la pantalla (en el "Monitor Serial" del IDE). El número
  // 9600 es la "velocidad" de esa comunicación (tiene que coincidir con la
  // que configures en el Monitor Serial al abrirlo).

  Serial.println("Sistema iniciado.");
  // Serial.println() manda un mensaje de texto a la computadora y agrega
  // un salto de línea al final. Es solo para que TÚ (la persona) veas que
  // el programa arrancó bien; el Arduino no "necesita" este mensaje para
  // funcionar.
}

// ================================================================
// FUNCIÓN loop()
// A diferencia de setup(), esta función se repite UNA Y OTRA VEZ sin
// parar, mientras el Arduino esté encendido. Aquí vive toda la lógica
// del sistema.
// ================================================================
void loop() {

  bool raining = (digitalRead(RAIN_DO_PIN) == LOW);
  // "bool" es un tipo de variable que solo puede valer verdadero (true) o
  // falso (false). digitalRead() lee el voltaje actual de un pin y
  // devuelve HIGH o LOW. Aquí preguntamos: "¿el sensor de lluvia está en
  // LOW ahorita mismo?" — recuerda que este sensor manda LOW cuando SÍ
  // detecta agua. Si es LOW, "raining" queda en true (está lloviendo);
  // si no, queda en false.

  bool atOpenLimit  = (digitalRead(LIMIT_OPEN)  == LOW);
  bool atCloseLimit = (digitalRead(LIMIT_CLOSE) == LOW);
  // Igual que arriba, pero preguntando si cada switch está siendo
  // presionado AHORA MISMO (recuerda: con INPUT_PULLUP, "presionado" = LOW).

  // --- Bloque: medir CUÁNTO TIEMPO lleva lloviendo o seco sin parar ---

  if (raining) { if (rainSince == 0) rainSince = millis(); drySince = 0; }
  else         { if (drySince  == 0) drySince  = millis(); rainSince = 0; }
  // millis() es una función que devuelve "cuántos milisegundos han pasado
  // desde que el Arduino se encendió". La usamos como un cronómetro.
  //
  // Esta línea dice, en español llano:
  //   "Si está lloviendo AHORA: si todavía no habíamos anotado desde
  //    cuándo empezó a llover (rainSince sigue en 0), anota el momento
  //    actual. Y borra el cronómetro de sequedad (drySince = 0), porque
  //    ya no aplica."
  //   "Si NO está lloviendo AHORA: haz lo mismo pero al revés."
  //
  // El truco de "si todavía vale 0, anótalo" es lo que hace que rainSince
  // guarde el momento en que EMPEZÓ a llover, y no se actualice a cada
  // rato mientras sigue lloviendo (si no, nunca podríamos medir "cuánto
  // tiempo lleva" lloviendo sin parar).

  bool rainConfirmed = rainSince && (millis() - rainSince > RAIN_CONFIRM_MS);
  // Esto calcula: "¿ya pasaron más de 3 segundos (RAIN_CONFIRM_MS) desde
  // que empezó a llover?" El "rainSince &&" al inicio es una protección:
  // en programación, cualquier número distinto de 0 se considera
  // "verdadero", y 0 se considera "falso". Entonces esa parte dice
  // "solamente si rainSince no es 0" (o sea, solo si SÍ está lloviendo
  // ahora), evaluamos la resta de tiempos. Esto evita hacer cálculos raros
  // cuando no está lloviendo.

  bool dryConfirmed  = drySince  && (millis() - drySince  > DRY_CONFIRM_MS);
  // Lo mismo que la línea anterior, pero para confirmar 30 segundos secos.

  // --- Bloque: decidir si hay que EMPEZAR a mover el techo ---

  if (rainConfirmed && state != CLOSED && state != CLOSING) {
    // "&&" significa "Y" (las dos condiciones tienen que cumplirse a la vez).
    // "!=" significa "diferente de". Esta línea dice:
    // "Si ya se confirmó que está lloviendo, Y el techo NO está ya cerrado,
    //  Y tampoco está YA en proceso de cerrarse... entonces hay que
    //  empezar a cerrarlo ahora."
    // (Sin este chequeo, el programa intentaría "volver a cerrar" algo que
    //  ya está cerrado, una y otra vez, sin necesidad.)

    state = CLOSING;
    // Cambiamos el estado guardado en la variable "state" a CLOSING.

    moveStartTime = millis();
    // Anotamos el momento exacto en que arrancó este movimiento, para
    // poder medir después cuánto tiempo lleva (y detectar si se tarda
    // demasiado, con MAX_MOVE_MS).

    Serial.println("Lluvia detectada -> cerrando techo");
    // Mensaje informativo para ti, no afecta el funcionamiento.
  }

  if (dryConfirmed && state != OPEN && state != OPENING) {
    // Misma lógica que el bloque anterior, pero para el caso contrario:
    // si ya se confirmó que lleva tiempo seco, y el techo no está ya
    // abierto ni abriéndose, entonces hay que empezar a abrirlo.
    state = OPENING;
    moveStartTime = millis();
    Serial.println("Sin lluvia -> abriendo techo");
  }

  // --- Bloque: mientras se está moviendo, revisar si ya llegó, o si
  //     se está tardando demasiado ---

  switch (state) {
  // "switch" es como una serie de "si state es esto, haz esto otro,
  // si state es lo otro, haz otra cosa". Es más ordenado que escribir
  // muchos "if" seguidos cuando hay varias opciones posibles.

    case CLOSING:
      // Este bloque de código solo se ejecuta si state vale CLOSING.

      if (atCloseLimit) {
        // Si el switch de "cerrado" está siendo presionado ahora mismo...

        stopMotor();
        // ...apagamos el motor inmediatamente...

        state = CLOSED;
        // ...y actualizamos el estado: ya llegamos, el techo está cerrado.

        Serial.println("Techo cerrado (switch activado)");

      } else if (millis() - moveStartTime > MAX_MOVE_MS) {
        // Si NO se ha activado el switch todavía, pero ya pasaron más de
        // 8 segundos moviéndose (comparamos el momento actual "millis()"
        // contra el momento en que arrancó el movimiento, "moveStartTime")...

        stopMotor();
        // ...apagamos el motor por seguridad, aunque no haya llegado...

        Serial.println("ERROR: tardó demasiado en cerrar, revisa el mecanismo o el switch");
        // ...y avisamos que algo anda mal, para que lo revises.
        // (Nota: aquí no cambiamos "state", se queda en CLOSING, así el
        //  programa sabe que quedó a medias y no asume que está cerrado.)

      } else {
        // Si no pasó ninguna de las dos cosas anteriores (todavía no llega
        // y todavía no se tarda demasiado), seguimos moviendo el motor.

        driveMotor(false);
        // Llamamos a la función driveMotor() (definida más abajo) pidiendo
        // que gire en dirección "cerrar" (false = dirección de cierre).
      }
      break;
      // "break" corta la ejecución del switch aquí, para que no siga
      // revisando los demás "case" de abajo sin necesidad.

    case OPENING:
      // Exactamente la misma lógica que CLOSING, pero para la dirección
      // contraria (abrir), usando el switch de "abierto" en vez del de
      // "cerrado".
      if (atOpenLimit) {
        stopMotor();
        state = OPEN;
        Serial.println("Techo abierto (switch activado)");
      } else if (millis() - moveStartTime > MAX_MOVE_MS) {
        stopMotor();
        Serial.println("ERROR: tardó demasiado en abrir, revisa el mecanismo o el switch");
      } else {
        driveMotor(true);
        // true = dirección de apertura.
      }
      break;

    default:
      // "default" es lo que se ejecuta si state NO es ninguno de los
      // casos de arriba (o sea, si state es OPEN o CLOSED: el techo está
      // quieto en una posición final, no en movimiento).
      stopMotor();
      // Nos aseguramos de que el motor esté apagado mientras no haya
      // ninguna razón para moverlo.
      break;
  }
}

// ================================================================
// FUNCIÓN driveMotor()
// Esta función NO se llama sola; la llamamos nosotros desde loop()
// cada vez que queremos que el motor gire. Recibe un dato de entrada
// (un parámetro) llamado "openDir" que nos dice hacia dónde girar.
// ================================================================
void driveMotor(bool openDir) {
  // "bool openDir" significa: esta función espera que le mandes un
  // verdadero/falso. true = "quiero que abra", false = "quiero que cierre".

  digitalWrite(IN1, openDir ? HIGH : LOW);
  // digitalWrite() pone un pin en HIGH o en LOW (a diferencia de
  // digitalRead(), que LEE, digitalWrite() ESCRIBE/manda una señal).
  //
  // "openDir ? HIGH : LOW" es una forma corta de escribir un "if/else"
  // en una sola línea. Se lee así: "si openDir es true, usa HIGH; si es
  // false, usa LOW". Es exactamente lo mismo que escribir:
  //   if (openDir) { digitalWrite(IN1, HIGH); } else { digitalWrite(IN1, LOW); }
  // pero más corto.

  digitalWrite(IN2, openDir ? LOW  : HIGH);
  // IN2 siempre recibe la señal CONTRARIA a IN1. Así es como funciona un
  // "puente H" como el L298N: la combinación de HIGH/LOW en estos dos
  // cables es lo que define si el motor gira en un sentido o en el otro.

  analogWrite(ENA, MOTOR_SPEED);
  // analogWrite() no es un simple HIGH/LOW: manda una señal PWM, que es
  // como "prender y apagar muy rápido" el pin para simular un voltaje
  // intermedio. Así es como controlamos la VELOCIDAD del motor (0 a 255).
  // Aquí usamos el valor fijo que definimos al principio (MOTOR_SPEED = 200).
}

// ================================================================
// FUNCIÓN stopMotor()
// Función simple para apagar el motor por completo.
// ================================================================
void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  // Ponemos ambos cables de dirección en LOW: sin una combinación
  // HIGH/LOW válida, el motor no tiene "orden" de girar en ningún sentido.

  analogWrite(ENA, 0);
  // Y por si acaso, ponemos la velocidad en 0 también (equivale a apagado).
  // Hacer las dos cosas (dirección en LOW y velocidad en 0) es una
  // doble seguridad para que el motor definitivamente no se mueva.
}