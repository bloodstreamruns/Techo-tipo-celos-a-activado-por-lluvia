# Techo automático que se cierra cuando llueve

Esta guía está escrita para alguien que nunca ha armado un circuito con Arduino. Vamos a explicar qué hace cada pieza, por qué se conecta donde se conecta, y qué hace el programa una vez que todo está armado.

## ¿Qué hace este proyecto, en términos simples?

Imagina un techo tipo persiana o celosía, de esos que se pueden plegar para abrir o cerrar. Queremos que:

- Si empieza a llover, el techo se **cierre solo**.
- Si deja de llover un rato, el techo se **abra solo** de nuevo.

Para lograr esto necesitamos 3 cosas trabajando juntas:
1. Algo que **detecte** si está lloviendo (el sensor de lluvia).
2. Algo que **mueva** el mecanismo del techo (el motor).
3. Algo que **decida** qué hacer y cuándo (el Arduino, siguiendo el programa).

## Las piezas que vamos a usar (y qué hace cada una)

| Pieza | ¿Qué es? | ¿Para qué sirve aquí? |
|---|---|---|
| Arduino UNO | Una pequeña computadora programable | Es el "cerebro": lee los sensores y decide cuándo mover el motor |
| Sensor de lluvia (FC-37 / YL-83) | Una placa con líneas metálicas expuestas | Cuando el agua toca las líneas, cambia una señal eléctrica que el Arduino puede leer |
| L298N | Un módulo con dos terminales azules y un disipador de metal | El Arduino por sí solo no tiene fuerza eléctrica para mover un motor; el L298N recibe una orden pequeña del Arduino y la convierte en la corriente más fuerte que el motor necesita |
| Motorreductor (motor amarillo con cables) | Un motor con engranajes que le dan más fuerza pero menos velocidad | Es lo que físicamente mueve el mecanismo del techo |
| 2 microswitches (fin de carrera) | Pequeños interruptores de botón | Le avisan al Arduino "ya llegué al final del recorrido", uno para cuando el techo termina de abrir y otro para cuando termina de cerrar |
| Batería o fuente externa (6-12V) | Una fuente de energía aparte | El motor necesita más corriente de la que el Arduino puede dar; por eso se alimenta aparte |

## Antes de empezar: ideas básicas que necesitas entender

- **Un cable a GND es como conectar algo a "tierra"**: es el punto de referencia de 0 voltios que todo el circuito comparte. Sin un GND en común entre todas las piezas, nada funciona bien, aunque todo esté "conectado".
- **Los pines del Arduino tienen un número** (D2, D8, D9, D10, etc.), impreso justo al lado de cada hueco donde se mete un cable. Vamos a usar esos números para saber exactamente dónde va cada cable.
- **El motor necesita su propia energía.** El Arduino no puede alimentar un motor directamente desde sus pines de 5V: no tiene la fuerza suficiente y puede dañarse. Por eso el motor se alimenta con una batería aparte, y solo se "avisan" entre sí compartiendo el cable de GND.

## Paso 1: conectar el sensor de lluvia

El sensor tiene 4 conexiones marcadas en la placa: VCC, GND, DO, y AO.

1. Un cable del hueco marcado **VCC** del sensor → al pin marcado **5V** en el Arduino.
2. Un cable del hueco marcado **GND** del sensor → a cualquier hueco marcado **GND** en el Arduino.
3. Un cable del hueco marcado **DO** del sensor → al pin digital **2** del Arduino.
4. El hueco marcado **AO** del sensor: no lo conectamos por ahora, se deja libre.

¿Por qué DO y no AO? DO te da una respuesta simple de "sí hay agua / no hay agua", que es justo lo que necesita este proyecto. AO da un valor más detallado (qué tanta agua hay), que no usamos en esta versión.

## Paso 2: conectar el L298N al Arduino

El L298N tiene una fila de pines pequeños marcados algo así: `IN1 IN2 IN3 IN4 ENA ENB` (el orden exacto puede variar un poco según el modelo, revisa lo que dice impreso en tu placa).

1. Cable desde **IN1** del L298N → pin digital **8** del Arduino.
2. Cable desde **IN2** del L298N → pin digital **9** del Arduino.
3. Cable desde **ENA** del L298N → pin digital **10** del Arduino (este pin tiene un símbolo `~` al lado en el Arduino; ese símbolo indica que puede controlar velocidad, no solo encendido/apagado).
4. **Importante:** el módulo L298N normalmente trae puesto un pequeño puente de plástico ("jumper") sobre los pines ENA/ENB. Si lo dejas puesto, el motor siempre irá a máxima velocidad y no podrás controlar qué tan rápido gira. **Quita ese jumper** para poder usar el cable de ENA que conectamos arriba.
5. Un cable de **GND** del lado de los pines de control del L298N → a un **GND** del Arduino.

## Paso 3: conectar el motor y la alimentación al L298N

El L298N tiene, aparte de los pines pequeños, un bloque de terminales de tornillo más grandes, generalmente en dos grupos: uno para el motor ("OUT1"/"OUT2" o similar) y otro para la alimentación de potencia ("+12V" y "GND").

1. Los **dos cables del motor amarillo** → a los dos tornillos del bloque de salida de motor ("Motor A" o similar). El orden entre los dos cables no importa todavía; si luego el motor gira al revés de lo esperado, simplemente se intercambian estos dos cables.
2. El cable **positivo (+)** de tu batería/fuente externa → al tornillo marcado como entrada de alimentación positiva (a veces dice "+12V" aunque uses menos voltaje, como 6-9V).
3. El cable **negativo (-)** de esa misma batería → al tornillo de GND de ese mismo bloque de alimentación.
4. **Muy importante:** ese mismo GND de la batería también debe unirse, con otro cable, al GND del Arduino. Sin este paso, el Arduino y el motor "no se entienden" entre sí aunque parezca que todo está bien conectado.

**Regla de oro:** el Arduino se alimenta por su cable USB (o su propia fuente), y el motor se alimenta por la batería aparte. Nunca conectes el pin de 5V del Arduino para darle energía al motor.

## Paso 4: conectar los dos microswitches (fin de carrera)

Cada microswitch normalmente tiene 2 o 3 patitas metálicas marcadas COM, NA y NC. Para este proyecto solo necesitas usar 2 de esas patitas: **COM** y **NC** (o COM y NA, cualquiera de las dos combinaciones funciona, lo que cambia es si el switch "avisa" al soltarse o al presionarse — para la lógica de este proyecto, cualquiera sirve).

**Switch de "techo abierto":**
1. Una pata del switch → a un GND del Arduino.
2. La otra pata → al pin digital **4** del Arduino.

**Switch de "techo cerrado":**
1. Una pata del switch → a un GND del Arduino.
2. La otra pata → al pin digital **5** del Arduino.

No necesitas agregar ninguna resistencia extra para estos switches — el programa ya le indica al Arduino que use una resistencia que trae integrada (esto se llama `INPUT_PULLUP` dentro del código, pero como usuario no necesitas hacer nada adicional para eso, ya está resuelto en el programa).

**Dónde colocar cada switch físicamente:** cada uno debe quedar en el punto exacto donde el mecanismo del techo termina su recorrido — uno en el extremo de "completamente abierto", y el otro en el extremo de "completamente cerrado" — de forma que el propio brazo, leva o marco del techo presione el switch justo al llegar ahí. Si el switch está mal posicionado (muy adelantado o muy atrasado), el techo se detendrá antes o después del punto correcto.

## Resumen visual de todas las conexiones

```
                         ARDUINO UNO
                    ┌─────────────────────┐
   Sensor lluvia    │                     │
   VCC ────────────►│ 5V                  │
   GND ────────────►│ GND                 │
   DO  ────────────►│ D2                  │
                     │                     │
   Switch "abierto"  │                     │
   (una pata) ──────►│ GND                 │
   (otra pata) ─────►│ D4                  │
                     │                     │
   Switch "cerrado"  │                     │
   (una pata) ──────►│ GND                 │
   (otra pata) ─────►│ D5                  │
                     │                     │
   L298N IN1  ◄──────│ D8                  │
   L298N IN2  ◄──────│ D9                  │
   L298N ENA  ◄──────│ D10 (~)             │
   L298N GND  ◄──────│ GND                 │
                     └─────────────────────┘

                         MÓDULO L298N
                    ┌─────────────────────┐
   Motor (2 cables) ◄│ OUT1 / OUT2         │
   Batería + ───────►│ +V (bloque grande)  │
   Batería - ───────►│ GND (bloque grande) │──── también va unido al GND del Arduino
                     └─────────────────────┘
```

## Cómo funciona el sistema, explicado sin tecnicismos

1. El Arduino, muchas veces por segundo, se pregunta: "¿el sensor de lluvia me está diciendo que hay agua ahora mismo?"

2. No actúa de inmediato apenas detecta una gota. Espera **3 segundos seguidos** de lluvia detectada antes de decidir "sí, definitivamente está lloviendo". Esto evita que el techo se cierre por una sola gota que cayó por casualidad.

3. De la misma manera, cuando deja de llover, el Arduino espera **30 segundos seguidos** sin lluvia antes de decidir "ya se secó, puedo abrir otra vez". Esto evita que el techo esté abriendo y cerrando todo el tiempo si la lluvia va y viene.

4. Una vez que se confirma "sí está lloviendo" (o "ya se secó"), el Arduino le manda al L298N la orden de encender el motor en la dirección correcta (cerrar o abrir).

5. El motor se mueve, y el Arduino va revisando en cada instante si el microswitch correspondiente ya se activó. En cuanto eso pasa, apaga el motor de inmediato — ese es el momento exacto en que el techo terminó de moverse.

6. Como medida de seguridad, si el motor lleva más de **8 segundos** moviéndose y el switch todavía no se activó, el Arduino asume que algo salió mal (el mecanismo se atoró, un cable se soltó, el switch está mal puesto) y apaga el motor de todos modos, para no forzarlo ni quemarlo.

## Qué revisar si algo no funciona

| Lo que ves | Qué probablemente pasa |
|---|---|
| El motor no se mueve nunca | Revisa que la batería esté conectada y con carga; revisa que el GND de la batería esté unido al GND del Arduino (paso que se olvida fácil) |
| El motor gira, pero siempre en el mismo sentido | Revisa que IN1 esté en D8 e IN2 esté en D9, sin cruzarlos |
| El motor no se detiene al llegar al final | El switch correspondiente no se está presionando en el punto correcto, o los cables están en el pin equivocado |
| El techo se mueve al revés de lo que esperabas | Intercambia los dos cables del motor en las terminales del L298N |
| El Arduino se reinicia o se apaga solo justo cuando el motor arranca | Le está faltando corriente porque comparte fuente con el motor; verifica que estén separados y solo compartan el cable de GND |
| El sensor de lluvia no reacciona al agua | Tiene un tornillito de ajuste (potenciómetro) en la placa; gíralo un poco y vuelve a probar mojando la placa sensora directamente |
