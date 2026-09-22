/*
 * ============================================================
 *  POLO — ROBOT CAMINANTE — ESP32-C3 Super Mini — v7.1
 *
 *  NOVEDADES DE v7.1: PINES DE SERVOS SEGÚN LA PCB
 *   En la PCB ensamblada los servos quedaron en otros pines que en el
 *   prototipo. El mapeo correcto ahora es:
 *       Pata izquierda → GPIO2
 *       Pata derecha   → GPIO4
 *       Balancín       → GPIO3
 *   Solo cambian los pines. El orden en la interfaz, los índices
 *   internos y los valores guardados siguen siendo los mismos, así que
 *   cada control vuelve a mover el servo que dice su nombre.
 *
 *  NOVEDADES DE v7: ESTRIDULACIÓN Y OJOS
 *   · Buzzer pasivo en GPIO7 que simula el raspado de un escarabajo.
 *     Tres modos: continuo, un chirrido por paso, o al arrancar y parar.
 *     Cuatro presets de partida y todos los parámetros ajustables.
 *   · Dos ojos LED independientes en GPIO10 y GPIO1, con latido de dos
 *     picos tipo lub-dub, corrección de gamma y opción de sincronizarse
 *     con la marcha: un latido por paso.
 *   · Todo se guarda en la memoria del micro, así que cada quien deja
 *     su propia configuración de sonido y de ojos.
 *   · Interruptores de sonido y ojos en la pantalla de manejo.
 *
 *  CABLEADO DE LO NUEVO
 *     Buzzer pasivo (módulo de 3 pines):  S → GPIO7 · centro → 5V · − → GND
 *     Ojo izquierdo:  +5V ── LED ── 220 Ω ── GPIO10
 *     Ojo derecho:    +5V ── LED ── 220 Ω ── GPIO1
 *   Los LEDs cuelgan del riel de 5 V y el pin drena la corriente. El
 *   código ya invierte la lógica y para apagarlos del todo suelta el
 *   canal y deja el pin en entrada, que es lo que garantiza que
 *   funcionen con LEDs de cualquier color.
 *
 *  Dos vistas:
 *   · MANEJO (pantalla principal): cruceta de 4 flechas y 5 niveles
 *     de velocidad. Mantener presionada una flecha hace caminar,
 *     soltarla devuelve el robot a reposo.
 *   · CONFIGURACIÓN: calibración, recorridos, fases, forma de onda,
 *     gráfica del ciclo y ajustes finos.
 *
 *  FORMA DE ONDA POR SERVO (novedad de v4)
 *   El balancín es un mecedor simétrico: no tiene carrera de apoyo ni
 *   de vuelo, las dos mitades del ciclo cumplen la misma función. Una
 *   onda asimétrica le da más tiempo de suelo a un lado que al otro,
 *   y eso se ve exactamente como una cojera. Por eso ahora cada servo
 *   elige su forma:
 *     · BALANCEO — simétrica, con pausa en los extremos. Mantiene la
 *       inclinación mientras las patas levantadas avanzan, y cambia
 *       de lado rápido. (Para el balancín.)
 *     · EMPUJE   — barrido de velocidad casi constante en la carrera
 *       cargada y retorno suavizado, ambas mitades de igual duración.
 *       (Para los servos de patas.)
 *
 *   Las patas de este robot no se levantan solas: las levanta el
 *   balancín, y lo hace en un reparto estricto de 50/50. Por eso las
 *   carreras de apoyo y vuelo de cada pata duran exactamente medio
 *   ciclo cada una, y lo único que se puede cambiar es su forma.
 *
 *  SALIDA DE MONITOREO PARA OSCILOSCOPIO (novedad de v5)
 *   Tres pines nuevos que no mueven nada — solo permiten ver el
 *   movimiento en un osciloscopio:
 *     GPIO5  → canal 1   |  GPIO6  → canal 2
 *       PWM de 20 kHz cuyo ciclo de trabajo sigue el ángulo real
 *       comandado al servo. Con un filtro RC (4,7 kΩ + 1 µF) se
 *       convierte en una tensión de 0 a 3,3 V: la onda de marcha
 *       tal cual, lista para medir con los cursores.
 *     GPIO7  → sincronía
 *       Onda cuadrada de un pulso por ciclo. Va directo a la sonda,
 *       sin filtro, y sirve para disparar el osciloscopio de forma
 *       estable. Alta durante la primera mitad del ciclo.
 *
 *   Qué servo sale por cada canal se elige en Ajustes finos.
 *   La escala es automática: cada canal usa el recorrido del servo
 *   más un 25 % de margen, así la onda ocupa casi toda la pantalla
 *   sin llegar nunca a los topes.
 *
 *  Librerías (Gestor de librerías del IDE):
 *   - ESP32Servo   (Kevin Harrington / John K. Bennett)
 *   - WebSockets   (Markus Sattler)
 *  Placa: "ESP32C3 Dev Module" · USB CDC On Boot: Enabled
 *
 *  Uso: conectarse a la red "POLO" (clave robot1234)
 *       y abrir http://192.168.4.1
 *
 *  NOTA: las rutinas guardadas en v4 siguen siendo válidas. Solo se
 *  reinician los ajustes finos, porque su estructura creció.
 * ============================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESP32Servo.h>
#include <Preferences.h>
#include <WebSocketsServer.h>

// ==================================================
//  RED (modo AP)
// ==================================================
const char* AP_SSID  = "POLO";
const char* AP_PASS  = "robot1234";     // mínimo 8 caracteres
const int   AP_CANAL = 6;               // 1, 6 u 11 — usar el más limpio
const int   AP_MAX_CLIENTES = 2;

IPAddress AP_IP(192, 168, 4, 1);
IPAddress AP_MASK(255, 255, 255, 0);

DNSServer        dns;
WebServer        server(80);
WebSocketsServer ws(81);
Preferences      prefs;

// ==================================================
//  PINES
// ==================================================
/*  Mapeo según la PCB (v7.1). En el prototipo eran 4 / 3 / 2.
 *  Si algún día cambia el ruteo, este es el único lugar que hay que tocar:
 *  el resto del firmware y la interfaz trabajan por índice
 *  (0 = pata izquierda, 1 = pata derecha, 2 = balancín).
 */
#define PIN_PATA_IZQ 2
#define PIN_PATA_DER 4
#define PIN_BALANCIN 3
#define N_SERVOS 3

const int PINES[N_SERVOS] = {PIN_PATA_IZQ, PIN_PATA_DER, PIN_BALANCIN};

const int US_MIN = 500;    // ancho de pulso del SG90
const int US_MAX = 2400;

// --- ojos y buzzer ---
#define PIN_LED_I  10      // ojo izquierdo:  +5V - LED - 220Ω - GPIO10
#define PIN_LED_D  1       // ojo derecho:    +5V - LED - 220Ω - GPIO1
#define PIN_BUZZER 7       // buzzer pasivo (módulo de 3 pines: S aquí)

#define PWM_BITS 10
#define PWM_MAX  1023

/*  PRESUPUESTO DE CANALES PWM
 *  El ESP32-C3 tiene exactamente 6 canales LEDC y aquí se usan los 6:
 *      3 servos + 2 ojos + 1 buzzer
 *  Por eso las salidas de monitoreo para osciloscopio de v5/v6 están
 *  desactivadas: sus pines GPIO5 y GPIO6 pasaron a ser el bus I2C, y
 *  además no quedan canales libres.
 *
 *  Poner MONITOR_OSC en 1 las revive para filmar formas de onda, pero
 *  a cambio APAGA los ojos (libera sus dos canales) y deja el I2C
 *  inutilizable mientras esté activo. Es un modo de laboratorio.
 */
#define MONITOR_OSC 0

#if MONITOR_OSC
  #define PIN_MON_A    5
  #define PIN_MON_B    6
  #define MON_FREQ 20000
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    #define MON_CH_A PIN_MON_A
    #define MON_CH_B PIN_MON_B
  #else
    #define MON_CH_A 4
    #define MON_CH_B 5
  #endif
#endif

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  #define CH_LED_I  PIN_LED_I
  #define CH_LED_D  PIN_LED_D
  #define CH_BUZZER PIN_BUZZER
#else
  #define CH_LED_I  3        // los servos ocupan los canales LEDC 0-2
  #define CH_LED_D  4
  #define CH_BUZZER 5
#endif

// ==================================================
//  ESTRUCTURAS
// ==================================================
struct OscParam {
  float   angMin, angMax, fase;
  bool    activo, invertir;
  uint8_t forma;       // 0 = balanceo (simétrica), 1 = empuje (barrido + retorno)
};

struct Marcha {
  OscParam osc[N_SERVOS];
  uint16_t periodo;    // ms por ciclo (modo configuración y nivel "Normal")
  float    pausa;      // 0.00-0.60 — fracción del ciclo detenida en los extremos
  float    amplitud;   // 0.00-1.20 — escala global
};

struct Config {
  float    velMax[N_SERVOS];  // °/s máximo de cada servo, con su carga real
  uint16_t msBlend;           // transición marcha <-> reposo
  bool     silencio;          // corta el PWM cuando está quieto
  uint8_t  monA;              // servo que sale por el canal 1 del osciloscopio
  uint8_t  monB;              // servo que sale por el canal 2
};

/*  ESTRIDULACIÓN
 *  Un escarabajo no emite un tono: frota un rascador contra una lima de
 *  dientes y cada diente produce un chasquido. Lo que se oye es un tren
 *  de pulsos. De ahí las tres capas:
 *    portadora → el tono de cada chasquido (2-4 kHz, donde el piezo suena)
 *    pulsoHz   → cuántos chasquidos por segundo. Esta capa es la que
 *                convierte el pitido en raspado; es la que más importa.
 *    frase     → ráfagas agrupadas y silencio. Nada vivo chirría continuo.
 */
struct Sonido {
  uint16_t portadora;   // Hz
  uint8_t  varPct;      // % de azar en la portadora
  uint16_t pulsoHz;     // pulsos por segundo
  uint8_t  dutyPct;     // % encendido de cada pulso
  uint16_t chirpMs;     // duración de una ráfaga
  uint16_t gapMs;       // silencio entre ráfagas
  uint8_t  nChirps;     // ráfagas por frase
  uint16_t fraseMs;     // silencio entre frases
  int8_t   barridoPct;  // % que se mueve la portadora dentro de la ráfaga
  uint8_t  volPct;      // % de volumen
  uint8_t  modo;        // 0=continuo  1=un chirrido por paso  2=al arrancar y parar
  bool     activo;
};

struct Ojos {
  uint16_t bpm;         // latidos por minuto en reposo
  uint8_t  basePct;     // brillo entre latidos
  uint8_t  picoPct;     // brillo del pico
  bool     alterno;     // los dos ojos laten desfasados
  bool     conMarcha;   // caminando, el latido sigue el ritmo del paso
  bool     activo;
};

struct Nivel {
  const char* nombre;
  float facT;          // multiplica el periodo configurado por el usuario
  float facAmp;        // escala la amplitud
};

// 0 = más lento .. 4 = más rápido.
// "Normal" respeta exactamente el periodo guardado en configuración.
const Nivel NIVELES[5] = {
  { "Sigiloso",   2.60f, 0.62f },
  { "Lento",      1.60f, 0.95f },
  { "Normal",     1.00f, 1.00f },
  { "Rápido",     0.72f, 1.00f },
  { "Muy rápido", 0.50f, 1.00f }
};

// ==================================================
//  VALORES POR DEFECTO
// ==================================================
float anguloReposo[N_SERVOS] = {60, 111, 68};
float anguloActual[N_SERVOS] = {60, 111, 68};

Marcha marcha = {
  { { 53, 100,  75, true, false, 1 },   // Pata izquierda — empuje
    { 92, 150,  75, true, false, 1 },   // Pata derecha   — empuje
    { 60,  85,   0, true, false, 0 } }, // Balancín       — balanceo
  1400,       // periodo
  0.30f,      // pausa en extremos
  1.00f       // amplitud
};

/*  PROVISIONAL: estos tres valores son estimaciones de un SG90 con carga,
 *  no mediciones. El asistente de "Medir límite" los reemplaza con el
 *  valor real de cada servo. Al medirlos, copiarlos aquí para que salgan
 *  de fábrica en cualquier robot armado con este firmware.
 *      { pata izquierda, pata derecha, balancín }
 */
Config cfg = { {400.0f, 400.0f, 340.0f}, 400, true, 2, 0 };

//                portad var puls duty chirp gap n frase barr vol modo activo
Sonido snd = {      3200,  8,  90,  45,  220,  90, 3,  900, -12, 60,   1, true };
Ojos   ojos = { 72, 6, 100, false, true, true };

struct PresetSonido { const char* nombre; Sonido s; };
const PresetSonido PRESETS_SND[4] = {
  { "Chirrido", { 3200,  8,  90, 45, 220,  90, 3,  900, -12, 60, 1, true } },
  { "Rasgado",  { 2600, 15,  45, 55, 400, 150, 2, 1400, -25, 65, 1, true } },
  { "Clic",     { 4000,  5,  25, 20,  60, 200, 4, 1800,   0, 75, 1, true } },
  { "Zumbido",  { 2200, 20, 160, 60, 700,  60, 1, 1100, -35, 55, 1, true } }
};

const char* PRESET_IDS[4] = {"fw", "bk", "lf", "rt"};

// ==================================================
//  ESTADO
// ==================================================
Servo servos[N_SERVOS];
bool  enganchado[N_SERVOS] = {false, false, false};

float salida[N_SERVOS];
float objetivo[N_SERVOS];

bool    caminando      = false;
bool    modoConduccion = true;     // arranca en la pantalla de manejo
uint8_t nivelVel       = 2;        // "Normal"
bool    limitado       = false;    // el periodo pedido se recortó por saturación
char    dirActual      = 0;
uint8_t presetsBit     = 0;        // bit i = preset i guardado

float faseCiclo = 0.0f;            // 0..1, acumulada
float mezcla    = 0.0f;            // 0 = pose fija, 1 = marcha
float Tef       = 1400.0f;         // periodo efectivo en ms
float Aef       = 1.0f;            // amplitud efectiva
unsigned long tQuieto = 0;

// --- asistente de medición de límite ---
const float CAL_T_INI  = 2400.0f;  // ms por barrido al empezar
const float CAL_T_MIN  = 200.0f;   // ms por barrido al final
const float CAL_DECAY  = 0.955f;   // cuánto se acorta en cada barrido
const float CAL_MARGEN = 0.85f;    // se guarda el 85 % de lo marcado

int8_t calServo = -1;              // -1 = asistente apagado
float  calT     = CAL_T_INI;
float  calFase  = 0.0f;

// --- motor de estridulación ---
uint32_t sndTFase  = 0;
uint32_t sndTPulso = 0;
uint8_t  sndChirpN = 0;
bool     sndEnChirp = false;
bool     sndPulsoOn = false;
bool     sndFrase   = false;   // hay una frase en curso
bool     ledEnganchado = false;
float    faseLatido = 0.0f;
bool     caminabaAntes = false;

// ==================================================
//  PERSISTENCIA (NVS)
// ==================================================
void guardarReposo() {
  prefs.begin("robot", false);
  prefs.putBytes("m4_reposo", anguloReposo, sizeof(anguloReposo));
  prefs.end();
}
void guardarMarchaActual() {
  prefs.begin("robot", false);
  prefs.putBytes("m4_actual", &marcha, sizeof(marcha));
  prefs.end();
}
void guardarConfig() {
  prefs.begin("robot", false);
  prefs.putBytes("m6_cfg", &cfg, sizeof(cfg));
  prefs.end();
}
void guardarSonido() {
  prefs.begin("robot", false);
  prefs.putBytes("m7_snd", &snd, sizeof(snd));
  prefs.end();
}
void guardarOjos() {
  prefs.begin("robot", false);
  prefs.putBytes("m7_ojos", &ojos, sizeof(ojos));
  prefs.end();
}
void guardarPreset(int i) {
  prefs.begin("robot", false);
  prefs.putBytes(PRESET_IDS[i], &marcha, sizeof(marcha));
  prefs.end();
  presetsBit |= (1 << i);
}
bool cargarPreset(int i) {          // solo lectura, no desgasta la flash
  prefs.begin("robot", true);
  bool ok = prefs.getBytes(PRESET_IDS[i], &marcha, sizeof(marcha)) == sizeof(marcha);
  prefs.end();
  return ok;
}
void escanearPresets() {
  presetsBit = 0;
  prefs.begin("robot", true);
  for (int i = 0; i < 4; i++)
    if (prefs.getBytesLength(PRESET_IDS[i]) == sizeof(Marcha)) presetsBit |= (1 << i);
  prefs.end();
}
void cargarTodo() {
  prefs.begin("robot", true);
  prefs.getBytes("m4_reposo", anguloReposo, sizeof(anguloReposo));
  prefs.getBytes("m4_actual", &marcha, sizeof(marcha));
  prefs.getBytes("m6_cfg", &cfg, sizeof(cfg));
  prefs.getBytes("m7_snd", &snd, sizeof(snd));
  prefs.getBytes("m7_ojos", &ojos, sizeof(ojos));
  prefs.end();
  for (int i = 0; i < N_SERVOS; i++) {
    anguloActual[i] = anguloReposo[i];
    salida[i]       = anguloReposo[i];
    objetivo[i]     = anguloReposo[i];
  }
  escanearPresets();
}

// ==================================================
//  FORMAS DE ONDA
// ==================================================
float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
float clampAng(float a) { return clampf(a, 0.0f, 180.0f); }

float smoothstep(float u) {
  u = clampf(u, 0.0f, 1.0f);
  return u * u * (3.0f - 2.0f * u);
}

/*  Rampa de velocidad casi constante: acelera, mantiene, frena.
 *  Es lo que hace avanzar el cuerpo a ritmo parejo durante el apoyo.
 */
float rampaTrapecio(float t) {
  const float e = 0.25f;                    // fracción de aceleración en cada punta
  const float V = 1.0f / (1.0f - e);        // velocidad de crucero normalizada
  if (t <= 0.0f) return 0.0f;
  if (t >= 1.0f) return 1.0f;
  if (t < e)          return V * t * t / (2.0f * e);
  if (t < 1.0f - e)   return V * e * 0.5f + V * (t - e);
  float u = 1.0f - t;
  return 1.0f - V * u * u / (2.0f * e);
}

/*  Aplica la pausa en los extremos a un medio ciclo.
 *  t: 0..1 dentro del medio ciclo. Devuelve 0..1.
 */
float conPausa(float t, float p, bool suave) {
  p = clampf(p, 0.0f, 0.80f);
  float b = p * 0.5f;
  if (t <= b)          return 0.0f;
  if (t >= 1.0f - b)   return 1.0f;
  float u = (t - b) / (1.0f - p);
  return suave ? smoothstep(u) : rampaTrapecio(u);
}

/*  forma 0 = BALANCEO: las dos mitades son iguales y suaves.
 *      Arranca quieto, acelera, frena, se queda quieto, y vuelve igual.
 *  forma 1 = EMPUJE: primera mitad a velocidad casi constante (carrera
 *      cargada), segunda mitad de retorno suavizado. Igual duración.
 */
float ondaServo(float f, float pausa, uint8_t forma) {
  f = f - floorf(f);
  bool primera = (f < 0.5f);
  float t = (primera ? f : f - 0.5f) * 2.0f;
  if (forma == 0) {
    float v = conPausa(t, pausa, true);
    return primera ? v : 1.0f - v;
  }
  if (primera) return conPausa(t, pausa, false);
  return 1.0f - conPausa(t, pausa, true);
}

// ==================================================
//  SALIDA DE MONITOREO (solo con MONITOR_OSC en 1)
// ==================================================
#if MONITOR_OSC
void iniciarMonitor() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_MON_A, MON_FREQ, PWM_BITS);
  ledcAttach(PIN_MON_B, MON_FREQ, PWM_BITS);
#else
  ledcSetup(MON_CH_A, MON_FREQ, PWM_BITS);
  ledcAttachPin(PIN_MON_A, MON_CH_A);
  ledcSetup(MON_CH_B, MON_FREQ, PWM_BITS);
  ledcAttachPin(PIN_MON_B, MON_CH_B);
#endif
}
#endif

#if MONITOR_OSC
/*  Lleva el ángulo real de un servo a 0..1 usando su propio recorrido
 *  más un 25 % de margen. Así la onda ocupa casi toda la pantalla del
 *  osciloscopio sin tocar nunca los topes, sin importar si el servo
 *  recorre 25° o 60°. Si el servo no oscila, cae a la escala 0-180°.
 */
float normalizarServo(int i) {
  if (i < 0 || i >= N_SERVOS) return 0.0f;
  OscParam &o = marcha.osc[i];
  float rango = o.angMax - o.angMin;
  if (rango < 1.0f) return clampf(salida[i] / 180.0f, 0.0f, 1.0f);
  float centro = (o.angMin + o.angMax) * 0.5f;
  float medio  = rango * 0.5f * 1.25f;
  return clampf((salida[i] - (centro - medio)) / (2.0f * medio), 0.0f, 1.0f);
}
#endif

/*  Se llama en cada ciclo de 50 Hz, después de escribir los servos,
 *  así que refleja el ángulo realmente comandado — incluido el recorte
 *  del limitador de velocidad. Si la onda aparece aplanada en el
 *  osciloscopio, es que el servo está saturado de verdad.
 */
void actualizarMonitor() {
#if MONITOR_OSC
  uint32_t a = (uint32_t)(normalizarServo(cfg.monA) * PWM_MAX);
  uint32_t b = (uint32_t)(normalizarServo(cfg.monB) * PWM_MAX);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_MON_A, a);
  ledcWrite(PIN_MON_B, b);
#else
  ledcWrite(MON_CH_A, a);
  ledcWrite(MON_CH_B, b);
#endif
#endif
}

// ==================================================
//  ESTRIDULACIÓN
// ==================================================
void tonoOn(uint16_t hz) {
  uint32_t d = (uint32_t)(PWM_MAX * clampf(snd.volPct / 200.0f, 0.02f, 0.5f));
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(PIN_BUZZER, hz);
  ledcWrite(PIN_BUZZER, d);
#else
  ledcWriteTone(CH_BUZZER, hz);
  ledcWrite(CH_BUZZER, d);
#endif
}

void tonoOff() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_BUZZER, 0);
#else
  ledcWrite(CH_BUZZER, 0);
#endif
}

/*  Arranca una frase completa: nChirps ráfagas separadas por gapMs.
 *  En modo "por paso" se llama una vez por ciclo de marcha.
 */
void dispararFrase() {
  if (!snd.activo) return;
  sndFrase    = true;
  sndChirpN   = 0;
  sndEnChirp  = true;
  sndTFase    = micros();
  sndTPulso   = sndTFase;
}

void callarSonido() {
  sndFrase = false;
  sndEnChirp = false;
  if (sndPulsoOn) { tonoOff(); sndPulsoOn = false; }
}

/*  Motor de pulsos. Se llama en cada vuelta del loop porque la gatilla
 *  necesita resolución de milisegundo; no puede esperar al ciclo de 50 Hz.
 */
void actualizarSonido() {
  if (!snd.activo || !sndFrase) {
    if (sndPulsoOn) { tonoOff(); sndPulsoOn = false; }
    return;
  }

  uint32_t ahora  = micros();
  uint32_t enFase = ahora - sndTFase;

  if (sndEnChirp) {
    uint32_t dur = (uint32_t)snd.chirpMs * 1000UL;
    if (enFase >= dur) {
      tonoOff(); sndPulsoOn = false;
      sndEnChirp = false;
      sndTFase = ahora;
      sndChirpN++;
      return;
    }

    uint16_t hz = snd.pulsoHz < 5 ? 5 : snd.pulsoHz;
    uint32_t perPulso = 1000000UL / hz;
    uint32_t onPulso  = (uint32_t)(perPulso * clampf(snd.dutyPct / 100.0f, 0.05f, 0.95f));
    uint32_t dentro   = ahora - sndTPulso;
    if (dentro >= perPulso) { sndTPulso = ahora; dentro = 0; }

    bool quiero = (dentro < onPulso);
    if (quiero && !sndPulsoOn) {
      float avance = (float)enFase / (float)dur;
      float f = snd.portadora * (1.0f + (snd.barridoPct / 100.0f) * avance);
      if (snd.varPct > 0) {
        float r = (random(-1000, 1001) / 1000.0f) * (snd.varPct / 100.0f);
        f *= (1.0f + r);
      }
      tonoOn((uint16_t)clampf(f, 200.0f, 12000.0f));
      sndPulsoOn = true;
    } else if (!quiero && sndPulsoOn) {
      tonoOff();
      sndPulsoOn = false;
    }

  } else {
    bool finFrase = (sndChirpN >= (snd.nChirps < 1 ? 1 : snd.nChirps));
    if (finFrase) {
      // En modo continuo la frase se reencadena sola; en los otros dos
      // el sonido espera a que el movimiento lo vuelva a disparar.
      if (snd.modo == 0) {
        if (enFase >= (uint32_t)snd.fraseMs * 1000UL) dispararFrase();
      } else {
        sndFrase = false;
      }
      return;
    }
    if (enFase >= (uint32_t)snd.gapMs * 1000UL) {
      sndEnChirp = true;
      sndTFase   = ahora;
      sndTPulso  = ahora;
    }
  }
}

// ==================================================
//  LATIDO DE LOS OJOS
// ==================================================
void engancharLeds(bool on) {
#if MONITOR_OSC
  return;                       // en modo laboratorio los ojos ceden sus canales
#else
  if (on && !ledEnganchado) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(PIN_LED_I, 1000, PWM_BITS);
    ledcAttach(PIN_LED_D, 1000, PWM_BITS);
#else
    ledcSetup(CH_LED_I, 1000, PWM_BITS); ledcAttachPin(PIN_LED_I, CH_LED_I);
    ledcSetup(CH_LED_D, 1000, PWM_BITS); ledcAttachPin(PIN_LED_D, CH_LED_D);
#endif
    ledEnganchado = true;
  } else if (!on && ledEnganchado) {
    // Apagado real: se suelta el canal y el pin queda en entrada. Es lo
    // que garantiza que quede apagado con LEDs de cualquier color, incluso
    // los rojos, cuya tensión directa es baja.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcDetach(PIN_LED_I); ledcDetach(PIN_LED_D);
#else
    ledcDetachPin(PIN_LED_I); ledcDetachPin(PIN_LED_D);
#endif
    pinMode(PIN_LED_I, INPUT);
    pinMode(PIN_LED_D, INPUT);
    ledEnganchado = false;
  }
#endif
}

/*  Brillo 0..1 → duty, con dos correcciones:
 *   · gamma: el ojo percibe la luz de forma logarítmica; sin elevar al
 *     cuadrado, los desvanecidos se ven como saltos bruscos.
 *   · inversión: el pin drena la corriente, así que duty 0 = LED a tope.
 */
void escribirLed(int pin, int canal, float brillo) {
  if (!ledEnganchado) return;
  float g = clampf(brillo, 0.0f, 1.0f);
  g = g * g;
  uint32_t duty = (uint32_t)(PWM_MAX * (1.0f - g));
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, duty);
#else
  ledcWrite(canal, duty);
#endif
}

/*  Curva de latido: dos picos, uno fuerte y otro suave, como un lub-dub. */
float curvaLatido(float t) {
  t = t - floorf(t);
  float a = expf(-powf((t - 0.10f) / 0.055f, 2.0f));
  float b = 0.55f * expf(-powf((t - 0.28f) / 0.080f, 2.0f));
  return clampf(a + b, 0.0f, 1.0f);
}

void actualizarOjos(float dt) {
  if (!ojos.activo) { engancharLeds(false); return; }
  engancharLeds(true);

  float bpm = ojos.bpm;
  // Caminando, el corazón sigue el paso: un latido por ciclo de marcha.
  if (ojos.conMarcha && (caminando || mezcla > 0.001f) && Tef > 1.0f) {
    bpm = clampf(60000.0f / Tef, 20.0f, 300.0f);
  }

  float T = 60.0f / (bpm < 10.0f ? 10.0f : bpm);
  faseLatido += dt / T;
  faseLatido -= floorf(faseLatido);

  float base = ojos.basePct / 100.0f;
  float pico = ojos.picoPct / 100.0f;
  float bI = base + (pico - base) * curvaLatido(faseLatido);
  float bD = ojos.alterno ? base + (pico - base) * curvaLatido(faseLatido + 0.5f) : bI;

  escribirLed(PIN_LED_I, CH_LED_I, bI);
  escribirLed(PIN_LED_D, CH_LED_D, bD);
}

// ==================================================
//  MOTOR DE MOVIMIENTO
// ==================================================
float amplitudEfectiva() {
  float a = marcha.amplitud;
  if (modoConduccion) a *= NIVELES[nivelVel].facAmp;
  return clampf(a, 0.0f, 1.2f);
}

/*  Periodo más corto en el que ningún servo supera SU propio límite.
 *  El tramo más exigente recorre todo el rango en (1 - pausa) * T/2
 *  segundos, y el pico de una smoothstep es 1.5x su velocidad media:
 *      pico = 3 * recorrido / ((1 - pausa) * T)
 *  Manda el servo más exigido, no el promedio — si una pata recorre
 *  más que la otra, esa es la que fija el piso de todo el robot.
 */
float periodoMinimoSeguroMs() {
  float a = amplitudEfectiva();
  float libre = 1.0f - clampf(marcha.pausa, 0.0f, 0.80f);
  if (libre < 0.1f) libre = 0.1f;
  float Tmin = 0.20f;                       // piso absoluto, en segundos
  for (int i = 0; i < N_SERVOS; i++) {
    OscParam &o = marcha.osc[i];
    if (!o.activo) continue;
    float rec = (o.angMax - o.angMin) * a;
    float T   = 3.0f * rec / (cfg.velMax[i] * libre);
    if (T > Tmin) Tmin = T;
  }
  return Tmin * 1000.0f;
}

float periodoEfectivoMs() {
  if (!modoConduccion) { limitado = false; return (float)marcha.periodo; }
  float T    = (float)marcha.periodo * NIVELES[nivelVel].facT;
  float piso = periodoMinimoSeguroMs();
  limitado = (T < piso);
  if (limitado) T = piso;
  return clampf(T, 200.0f, 12000.0f);
}

void engancharServo(int i, bool on) {
  if (on && !enganchado[i]) {
    servos[i].setPeriodHertz(50);
    servos[i].attach(PINES[i], US_MIN, US_MAX);
    enganchado[i] = true;
  } else if (!on && enganchado[i]) {
    servos[i].detach();
    enganchado[i] = false;
  }
}

void despertar() {
  tQuieto = millis();
  for (int i = 0; i < N_SERVOS; i++) engancharServo(i, true);
}

void escribirServo(int i, float ang) {
  ang = clampAng(ang);
  salida[i] = ang;
  if (!enganchado[i]) return;
  float us = US_MIN + (ang / 180.0f) * (float)(US_MAX - US_MIN);
  servos[i].writeMicroseconds((int)lroundf(us));
}

/*  ASISTENTE DE MEDICIÓN
 *  Mueve un solo servo entre sus dos extremos, con su misma forma de
 *  onda, acortando el barrido en cada pasada. No pasa por el limitador
 *  de velocidad: la idea es justamente dejar que el servo se quede
 *  corto. Cuando deja de llegar a los topes o empieza a temblar, la
 *  persona marca, y de ese barrido sale su °/s real.
 */
float calVelocidadPedida() {
  if (calServo < 0) return 0.0f;
  OscParam &o = marcha.osc[calServo];
  float libre = 1.0f - clampf(marcha.pausa, 0.0f, 0.80f);
  if (libre < 0.1f) libre = 0.1f;
  return 3.0f * (o.angMax - o.angMin) / ((calT / 1000.0f) * libre);
}

void actualizarCalibracion(float dt) {
  OscParam &o = marcha.osc[calServo];
  float T = calT / 1000.0f;
  calFase += dt / T;
  while (calFase >= 1.0f) {                 // fin de barrido: acelerar
    calFase -= 1.0f;
    calT = fmaxf(calT * CAL_DECAY, CAL_T_MIN);
  }
  float centro = (o.angMin + o.angMax) * 0.5f;
  float amp    = (o.angMax - o.angMin) * 0.5f;
  float w      = ondaServo(calFase, marcha.pausa, o.forma);
  if (o.invertir) w = 1.0f - w;

  for (int i = 0; i < N_SERVOS; i++) {
    if (i == calServo) escribirServo(i, centro + amp * (2.0f * w - 1.0f));
    else               escribirServo(i, anguloReposo[i]);
  }
  actualizarMonitor();
}

void aMedir(int i) {
  if (i < 0 || i >= N_SERVOS) { calServo = -1; return; }
  if (marcha.osc[i].angMax - marcha.osc[i].angMin < 2.0f) return;  // sin recorrido
  caminando = false;
  dirActual = 0;
  mezcla    = 0.0f;
  despertar();
  calServo = (int8_t)i;
  calT     = CAL_T_INI;
  calFase  = 0.0f;
}

void aMarcar() {
  if (calServo < 0) return;
  cfg.velMax[calServo] = clampf(calVelocidadPedida() * CAL_MARGEN, 60.0f, 1200.0f);
  guardarConfig();
  calServo = -1;
  for (int i = 0; i < N_SERVOS; i++) anguloActual[i] = anguloReposo[i];
}

void aCancelarMedicion() {
  calServo = -1;
  for (int i = 0; i < N_SERVOS; i++) anguloActual[i] = anguloReposo[i];
}

void actualizarMovimiento(float dt) {
  if (calServo >= 0) { actualizarCalibracion(dt); return; }

  Tef = periodoEfectivoMs();
  Aef = amplitudEfectiva();

  if (caminando || mezcla > 0.001f) {
    float T = Tef / 1000.0f;
    if (T < 0.05f) T = 0.05f;
    float antes = faseCiclo;
    faseCiclo += dt / T;
    faseCiclo -= floorf(faseCiclo);
    // Modo "un chirrido por paso": la frase arranca en cada vuelta del ciclo.
    if (snd.activo && snd.modo == 1 && caminando && faseCiclo < antes) dispararFrase();
  }

  // Modo "al arrancar y parar": una frase en cada transición.
  if (snd.activo && snd.modo == 2 && caminando != caminabaAntes) dispararFrase();
  caminabaAntes = caminando;

  float paso = dt / (cfg.msBlend / 1000.0f);
  mezcla = clampf(mezcla + (caminando ? paso : -paso), 0.0f, 1.0f);
  float s = smoothstep(mezcla);

  for (int i = 0; i < N_SERVOS; i++) {
    OscParam &o = marcha.osc[i];
    float destino = anguloActual[i];
    if (o.activo && s > 0.0f) {
      float centro = (o.angMin + o.angMax) * 0.5f;
      float amp    = (o.angMax - o.angMin) * 0.5f * Aef;
      float w      = ondaServo(faseCiclo + o.fase / 360.0f, marcha.pausa, o.forma);
      if (o.invertir) w = 1.0f - w;
      float g = centro + amp * (2.0f * w - 1.0f);
      destino = anguloActual[i] * (1.0f - s) + g * s;
    }
    objetivo[i] = clampAng(destino);
  }

  bool quieto = true;
  for (int i = 0; i < N_SERVOS; i++) {
    float maxPaso = cfg.velMax[i] * dt;
    float d = objetivo[i] - salida[i];
    if (d >  maxPaso) d =  maxPaso;
    if (d < -maxPaso) d = -maxPaso;
    if (fabsf(d) > 0.02f) quieto = false;
    escribirServo(i, salida[i] + d);
  }

  if (cfg.silencio) {
    if (!quieto || caminando || mezcla > 0.001f) {
      tQuieto = millis();
      for (int i = 0; i < N_SERVOS; i++) engancharServo(i, true);
    } else if (millis() - tQuieto > 1500) {
      for (int i = 0; i < N_SERVOS; i++) engancharServo(i, false);
    }
  }

  actualizarMonitor();
}

// ==================================================
//  ACCIONES
// ==================================================
int idPreset(const String &id) {
  for (int i = 0; i < 4; i++) if (id == PRESET_IDS[i]) return i;
  return -1;
}

void irReposo() {
  caminando = false;
  dirActual = 0;
  despertar();
  for (int i = 0; i < N_SERVOS; i++) anguloActual[i] = anguloReposo[i];
}

bool aConducir(const String &id) {
  int i = idPreset(id);
  if (i < 0 || !(presetsBit & (1 << i))) return false;
  if (!cargarPreset(i)) return false;
  if (!caminando) faseCiclo = 0.0f;
  dirActual = id[0];
  despertar();
  caminando = true;
  return true;
}

void aSoltar() {
  caminando = false;
  dirActual = 0;
  for (int i = 0; i < N_SERVOS; i++) anguloActual[i] = anguloReposo[i];
}

void aModo(bool conduccion) {
  modoConduccion = conduccion;
  caminando = false;
  dirActual = 0;
  for (int i = 0; i < N_SERVOS; i++) anguloActual[i] = anguloReposo[i];
}

void aNivel(int n) { nivelVel = (uint8_t)constrain(n, 0, 4); }

void aSet(int i, float a) {
  if (i < 0 || i >= N_SERVOS) return;
  caminando = false;
  dirActual = 0;
  despertar();
  anguloActual[i] = clampAng(a);
}

void aOsc(int i, float mn, float mx, float fs, bool on, bool inv, int fm) {
  if (i < 0 || i >= N_SERVOS) return;
  OscParam &o = marcha.osc[i];
  o.angMin = clampAng(mn);
  o.angMax = clampAng(mx);
  if (o.angMax < o.angMin) { float t = o.angMin; o.angMin = o.angMax; o.angMax = t; }
  o.fase     = fmodf(fs, 360.0f);
  o.activo   = on;
  o.invertir = inv;
  o.forma    = (uint8_t)constrain(fm, 0, 1);
  guardarMarchaActual();
}

void aVel(int T)     { if (T >= 150 && T <= 10000) { marcha.periodo = T; guardarMarchaActual(); } }
void aAmp(float v)   { marcha.amplitud = clampf(v, 0.0f, 1.2f);  guardarMarchaActual(); }
void aPausa(float v) { marcha.pausa    = clampf(v, 0.0f, 0.60f); guardarMarchaActual(); }
void aCfg(bool sil, int ma, int mb) {
  cfg.silencio = sil;
  cfg.monA     = (uint8_t)constrain(ma, 0, N_SERVOS - 1);
  cfg.monB     = (uint8_t)constrain(mb, 0, N_SERVOS - 1);
  if (!sil) despertar();
  guardarConfig();
}

void aVelMax(int i, float v) {
  if (i < 0 || i >= N_SERVOS) return;
  cfg.velMax[i] = clampf(v, 60.0f, 1200.0f);
  guardarConfig();
}

// --- sonido ---
void aSndActivo(bool on) {
  snd.activo = on;
  if (!on) callarSonido();
  else if (snd.modo == 0) dispararFrase();
  guardarSonido();
}
void aSndModo(int m) {
  snd.modo = (uint8_t)constrain(m, 0, 2);
  callarSonido();
  if (snd.activo && snd.modo == 0) dispararFrase();
  guardarSonido();
}
void aSndPreset(int i) {
  i = constrain(i, 0, 3);
  uint8_t modo = snd.modo;            // el modo es del usuario, no del preset
  bool    act  = snd.activo;
  snd = PRESETS_SND[i].s;
  snd.modo = modo;
  snd.activo = act;
  callarSonido();
  if (snd.activo) dispararFrase();
  guardarSonido();
}
void aSndProbar() { if (snd.activo) dispararFrase(); }

void aSndParam(char k, long v) {
  switch (k) {
    case 'u': snd.pulsoHz    = constrain(v, 5, 300);   break;
    case 'd': snd.dutyPct    = constrain(v, 5, 95);    break;
    case 'f': snd.portadora  = constrain(v, 500, 8000); break;
    case 'v': snd.varPct     = constrain(v, 0, 60);    break;
    case 'b': snd.barridoPct = constrain(v, -60, 60);  break;
    case 'w': snd.volPct     = constrain(v, 3, 100);   break;
    case 'c': snd.chirpMs    = constrain(v, 20, 2000); break;
    case 'q': snd.gapMs      = constrain(v, 10, 2000); break;
    case 'n': snd.nChirps    = constrain(v, 1, 12);    break;
    case 'F': snd.fraseMs    = constrain(v, 50, 8000); break;
    default: return;
  }
  guardarSonido();
}

// --- ojos ---
void aOjosActivo(bool on) { ojos.activo = on; if (!on) engancharLeds(false); guardarOjos(); }
void aOjosParam(char k, long v) {
  switch (k) {
    case 'B': ojos.bpm     = constrain(v, 10, 300); break;
    case 'S': ojos.basePct = constrain(v, 0, 90);   break;
    case 'P': ojos.picoPct = constrain(v, 5, 100);  break;
    case 'A': ojos.alterno   = (v == 1); break;
    case 'M': ojos.conMarcha = (v == 1); break;
    default: return;
  }
  guardarOjos();
}

bool aPreset(char accion, const String &id) {
  int i = idPreset(id);
  if (i < 0) return false;
  if (accion == 'g') { guardarPreset(i); return true; }
  if (cargarPreset(i)) { guardarMarchaActual(); despertar(); caminando = true; return true; }
  return false;
}

// ==================================================
//  PÁGINA WEB
// ==================================================
const char PAGINA[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="es"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Robot caminante</title>
<style>
:root{
  --fondo:#0a0d16; --panel:#131826; --panel2:#1c2333; --linea:#2a3348;
  --tx:#e6ebf7; --mut:#7d88a6; --ac:#4ea8ff; --izq:#4ea8ff; --der:#ff8a5b; --bal:#5be5a0;
  --ok:#5be5a0; --alerta:#ff5d73; --amar:#ffc457;
}
*{box-sizing:border-box}
body{margin:0;background:var(--fondo);color:var(--tx);padding:12px 12px 40px;
  max-width:720px;margin-inline:auto;
  font-family:system-ui,-apple-system,'Segoe UI',sans-serif;-webkit-tap-highlight-color:transparent}
.mono{font-family:ui-monospace,'SF Mono',Menlo,Consolas,monospace;font-variant-numeric:tabular-nums}

body[data-v="manejo"] #vConfig{display:none}
body[data-v="config"] #vManejo{display:none}
body[data-v="manejo"] .soloConfig{display:none}
body[data-v="config"] .soloManejo{display:none}

header{display:flex;align-items:center;gap:10px;margin-bottom:14px}
header h1{font-size:1.05rem;margin:0;letter-spacing:-.01em}
header .lat{margin-left:auto;font-size:.7rem;color:var(--mut)}
header .lat b{color:var(--ok)}
.iconbtn{background:var(--panel2);border:1px solid var(--linea);color:var(--tx);
  border-radius:10px;padding:8px 12px;font-size:.8rem;font-weight:600;cursor:pointer;font-family:inherit}

/* ---------- VISTA MANEJO ---------- */
.pad{display:grid;grid-template-columns:repeat(3,1fr);grid-template-rows:repeat(3,1fr);
  gap:9px;width:min(84vw,330px);aspect-ratio:1;margin:6px auto 20px;touch-action:none}
.dir{background:var(--panel);border:1px solid var(--linea);border-radius:16px;color:var(--tx);
  font-size:1.7rem;line-height:1;cursor:pointer;display:flex;align-items:center;justify-content:center;
  user-select:none;-webkit-user-select:none;transition:background .08s,transform .08s}
.dir:disabled{opacity:.28;cursor:default}
.dir.on{background:var(--ok);color:#05261a;border-color:transparent;transform:scale(.96)}
.d-fw{grid-area:1/2} .d-lf{grid-area:2/1} .d-rt{grid-area:2/3} .d-bk{grid-area:3/2}
.nucleo{grid-area:2/2;border-radius:50%;background:var(--panel2);border:1px solid var(--linea);
  display:flex;flex-direction:column;align-items:center;justify-content:center;gap:2px;
  position:relative;overflow:hidden}
.nucleo b{font-size:.72rem;letter-spacing:.08em;z-index:1}
.nucleo small{font-size:.62rem;color:var(--mut);z-index:1}
.pulso{position:absolute;inset:0;border-radius:50%;background:var(--ok);opacity:0;transform:scale(.2)}

.vels{display:grid;grid-template-columns:repeat(5,1fr);gap:6px;margin-bottom:12px}
.vels button{background:var(--panel);border:1px solid var(--linea);color:var(--mut);
  border-radius:11px;padding:10px 2px;font-size:.66rem;font-weight:600;cursor:pointer;
  font-family:inherit;line-height:1.25;min-height:46px}
.vels button.sel{background:var(--ac);color:#04213f;border-color:transparent}
.pista{font-size:.7rem;color:var(--mut);text-align:center;line-height:1.5;margin:0}

/* ---------- VISTA CONFIG ---------- */
.barra{display:flex;gap:8px;align-items:center;margin-bottom:12px}
.chip{padding:5px 11px;border-radius:999px;font-size:.72rem;font-weight:700;letter-spacing:.04em}
.chip.cal{background:#1c2333;color:var(--mut)}
.chip.run{background:#0f3a28;color:var(--ok)}
button{font-family:inherit}
.btn{background:var(--panel2);color:var(--tx);border:1px solid var(--linea);border-radius:10px;
  padding:9px 13px;font-weight:600;font-size:.82rem;cursor:pointer}
.btn:active{transform:translateY(1px)}
.btn.ir{background:var(--ok);color:#05261a;border-color:transparent;flex:1}
.btn.parar{background:var(--alerta);color:#2d0009;border-color:transparent;flex:1}
.btn.guardar{background:var(--amar);color:#3a2600;border-color:transparent}

.card{background:var(--panel);border:1px solid var(--linea);border-radius:14px;padding:13px;margin-bottom:12px}
.card h2{font-size:.68rem;margin:0 0 11px;color:var(--mut);letter-spacing:.12em;text-transform:uppercase;font-weight:700}
.nota{font-size:.68rem;color:var(--mut);line-height:1.45;margin-top:8px}

#onda{width:100%;height:120px;display:block;border-radius:8px;background:#0c111c}
.leyenda{display:flex;gap:14px;margin-top:8px;font-size:.7rem;color:var(--mut);flex-wrap:wrap}
.leyenda i{display:inline-block;width:9px;height:3px;border-radius:2px;margin-right:5px;vertical-align:middle}

.fila{display:flex;align-items:center;gap:10px;margin:9px 0}
.fila label{min-width:104px;font-size:.78rem;color:var(--mut)}
input[type=range]{flex:1;accent-color:var(--ac);height:26px;background:transparent}
.val{min-width:58px;text-align:right;font-size:.85rem;font-weight:700}
input[type=number],select{width:100%;background:var(--panel);color:var(--tx);border:1px solid var(--linea);
  border-radius:8px;padding:7px 4px;text-align:center;font-family:inherit;font-size:.85rem}
input[type=number]:focus,select:focus{outline:none;border-color:var(--ac)}

.servo{border-left:3px solid var(--linea);padding-left:11px;margin:14px 0}
.servo:first-child{margin-top:0}
.servo.s0{border-color:var(--izq)} .servo.s1{border-color:var(--der)} .servo.s2{border-color:var(--bal)}
.servo h3{margin:0 0 9px;font-size:.82rem;display:flex;align-items:center;gap:8px}
.servo h3 .ang{margin-left:auto;color:var(--mut);font-weight:400;font-size:.78rem}
.campos{display:grid;grid-template-columns:1fr 1fr 1fr 1.3fr;gap:7px}
.campos .k{font-size:.63rem;color:var(--mut);display:block;margin-bottom:3px;letter-spacing:.04em}
.sw{display:flex;gap:14px;margin-top:8px;font-size:.72rem;color:var(--mut);align-items:center}
.sw label{display:flex;align-items:center;gap:5px;cursor:pointer}
input[type=checkbox]{width:17px;height:17px;accent-color:var(--ok)}

.grid2{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.grid4{display:grid;grid-template-columns:repeat(4,1fr);gap:7px}
.grid4 .btn{padding:11px 4px;font-size:.78rem}

.toast{position:fixed;bottom:18px;left:50%;transform:translateX(-50%) translateY(8px);
  background:var(--ok);color:#05261a;font-weight:700;padding:10px 20px;border-radius:11px;
  font-size:.82rem;opacity:0;transition:.22s;pointer-events:none;z-index:9}
.toast.ver{opacity:1;transform:translateX(-50%) translateY(0)}
details summary{cursor:pointer;color:var(--mut);font-size:.72rem;list-style:none;padding:4px 0}
details summary::-webkit-details-marker{display:none}
details summary::before{content:"+ ";font-weight:700}
details[open] summary::before{content:"- "}
@media (prefers-reduced-motion:reduce){*{transition:none!important}}
</style></head><body data-v="manejo">

<header>
  <h1>Robot caminante</h1>
  <button class="iconbtn soloManejo" onclick="vista('config')">Configurar</button>
  <button class="iconbtn soloConfig" onclick="vista('manejo')">&larr; Manejo</button>
  <span class="lat mono">enlace <b id="lat">--</b></span>
</header>

<!-- ============ MANEJO ============ -->
<div id="vManejo">
  <div class="pad" id="pad">
    <button class="dir d-fw" data-d="fw">&#9650;</button>
    <button class="dir d-lf" data-d="lf">&#9664;</button>
    <div class="nucleo"><span class="pulso" id="pulso"></span>
      <b id="nEstado">REPOSO</b><small id="nVel">Normal</small></div>
    <button class="dir d-rt" data-d="rt">&#9654;</button>
    <button class="dir d-bk" data-d="bk">&#9660;</button>
  </div>
  <div class="vels" id="vels">
    <button data-v="0">Sigiloso</button>
    <button data-v="1">Lento</button>
    <button data-v="2">Normal</button>
    <button data-v="3">Rápido</button>
    <button data-v="4">Muy rápido</button>
  </div>
  <div class="grid2" style="margin-bottom:12px">
    <button class="btn" id="tgSnd" onclick="cmd('S,'+(st&&st.sOn?0:1),'/snd/on?v='+(st&&st.sOn?0:1))">Sonido</button>
    <button class="btn" id="tgOjo" onclick="cmd('O,'+(st&&st.oOn?0:1),'/ojos/on?v='+(st&&st.oOn?0:1))">Ojos</button>
  </div>
  <p class="pista" id="pista">Mantén presionada una flecha para caminar. Al soltarla vuelve a reposo.</p>
</div>

<!-- ============ CONFIGURACIÓN ============ -->
<div id="vConfig">

<div class="barra">
  <span id="chip" class="chip cal">REPOSO</span>
  <button id="btnMarcha" class="btn ir" onclick="toggleMarcha()">Iniciar marcha</button>
  <button class="btn" onclick="cmd('r','/reposo/ir')">Ir a reposo</button>
</div>

<div class="card">
  <h2>Ciclo de marcha</h2>
  <canvas id="onda"></canvas>
  <div class="leyenda">
    <span><i style="background:var(--izq)"></i>Izquierda</span>
    <span><i style="background:var(--der)"></i>Derecha</span>
    <span><i style="background:var(--bal)"></i>Balancín</span>
    <span><i style="background:var(--bal);opacity:.5"></i>Franja superior: lado apoyado</span>
  </div>
</div>

<div class="card">
  <h2>Ajuste global</h2>
  <div class="fila"><label>Periodo</label>
    <input type="range" id="per" min="250" max="4000" step="10"
      oninput="toco();lbl('perL',this.value+' ms')" onchange="cmd('v,'+this.value,'/vel?T='+this.value)">
    <span class="val mono" id="perL">--</span></div>
  <div class="fila"><label>Amplitud</label>
    <input type="range" id="amp" min="0" max="120" step="1"
      oninput="toco();lbl('ampL',this.value+' %')" onchange="cmd('a,'+(this.value/100),'/amp?v='+(this.value/100))">
    <span class="val mono" id="ampL">--</span></div>
  <div class="fila"><label>Pausa extremos</label>
    <input type="range" id="pau" min="0" max="60" step="1"
      oninput="toco();lbl('pauL',this.value+' %')" onchange="cmd('u,'+(this.value/100),'/pausa?v='+(this.value/100))">
    <span class="val mono" id="pauL">--</span></div>
  <div class="nota">La pausa es el tiempo que cada servo se queda quieto en sus extremos antes de volver. En el balancín sostiene la inclinación mientras las patas levantadas avanzan; sin ella el cuerpo empieza a bajar antes de que la pata haya terminado de adelantarse. Este periodo es también el que usa el nivel "Normal" en la pantalla de manejo.</div>
</div>

<div class="card">
  <h2>Recorrido por servo</h2>
  <div id="servos"></div>
  <div class="nota"><b>Balanceo</b>: las dos mitades del ciclo son iguales — arranca lento, acelera, frena y espera. Es la forma correcta para el balancín, que no distingue entre ida y vuelta.<br><b>Empuje</b>: la primera mitad avanza a velocidad casi constante para empujar el cuerpo a ritmo parejo, y la segunda vuelve suavizada. Es la forma para los servos de patas.</div>
</div>

<div class="card">
  <h2>Calibración y reposo</h2>
  <div id="cal"></div>
  <div class="grid2" style="margin-top:10px">
    <button class="btn" onclick="todos(90)">Todos a 90°</button>
    <button class="btn guardar" onclick="cmd('g','/reposo/guardar');toast('Reposo guardado')">Guardar como reposo</button>
  </div>
  <div class="nota">Mover un control detiene la marcha y deja el robot en esa pose.</div>
</div>

<div class="card">
  <h2>Guardar como rutina</h2>
  <div class="nota" style="margin:0 0 9px">El ajuste actual queda asignado a una de las cuatro flechas de la pantalla de manejo.</div>
  <div class="grid4">
    <button class="btn" onclick="pre('fw')">Adelante</button>
    <button class="btn" onclick="pre('bk')">Atrás</button>
    <button class="btn" onclick="pre('lf')">Izquierda</button>
    <button class="btn" onclick="pre('rt')">Derecha</button>
  </div>
  <div class="nota">Una flecha apagada en la pantalla de manejo es una rutina que aún no se ha guardado.</div>
</div>

<div class="card">
  <h2>Estridulación</h2>
  <div class="grid4">
    <button class="btn" onclick="cmd('P,0','/snd/preset?i=0')">Chirrido</button>
    <button class="btn" onclick="cmd('P,1','/snd/preset?i=1')">Rasgado</button>
    <button class="btn" onclick="cmd('P,2','/snd/preset?i=2')">Clic</button>
    <button class="btn" onclick="cmd('P,3','/snd/preset?i=3')">Zumbido</button>
  </div>
  <div class="fila" style="margin-top:12px"><label>Cuándo suena</label>
    <select id="sm" onchange="cmd('M,'+this.value,'/snd/modo?m='+this.value)">
      <option value="0">Continuo</option>
      <option value="1">Un chirrido por paso</option>
      <option value="2">Al arrancar y parar</option>
    </select>
    <button class="btn" style="padding:7px 11px" onclick="cmd('T','/snd/probar')">Probar</button>
  </div>

  <div class="fila"><label>Pulsos</label>
    <input type="range" id="su" min="15" max="220" step="1"
      oninput="toco();lbl('suL',this.value)" onchange="cmd('Z,u,'+this.value,'/snd/p?k=u&v='+this.value)">
    <span class="val mono" id="suL">--</span></div>
  <div class="fila"><label>Duración pulso</label>
    <input type="range" id="sd" min="10" max="90" step="1"
      oninput="toco();lbl('sdL',this.value+' %')" onchange="cmd('Z,d,'+this.value,'/snd/p?k=d&v='+this.value)">
    <span class="val mono" id="sdL">--</span></div>
  <div class="fila"><label>Portadora</label>
    <input type="range" id="sf" min="1500" max="5000" step="25"
      oninput="toco();lbl('sfL',this.value+' Hz')" onchange="cmd('Z,f,'+this.value,'/snd/p?k=f&v='+this.value)">
    <span class="val mono" id="sfL">--</span></div>
  <div class="fila"><label>Azar</label>
    <input type="range" id="sv" min="0" max="40" step="1"
      oninput="toco();lbl('svL',this.value+' %')" onchange="cmd('Z,v,'+this.value,'/snd/p?k=v&v='+this.value)">
    <span class="val mono" id="svL">--</span></div>
  <div class="fila"><label>Barrido</label>
    <input type="range" id="sb" min="-50" max="50" step="1"
      oninput="toco();lbl('sbL',this.value+' %')" onchange="cmd('Z,b,'+this.value,'/snd/p?k=b&v='+this.value)">
    <span class="val mono" id="sbL">--</span></div>
  <div class="fila"><label>Volumen</label>
    <input type="range" id="sw" min="5" max="100" step="1"
      oninput="toco();lbl('swL',this.value+' %')" onchange="cmd('Z,w,'+this.value,'/snd/p?k=w&v='+this.value)">
    <span class="val mono" id="swL">--</span></div>
  <div class="fila"><label>Ráfaga</label>
    <input type="range" id="sc" min="30" max="900" step="10"
      oninput="toco();lbl('scL',this.value+' ms')" onchange="cmd('Z,c,'+this.value,'/snd/p?k=c&v='+this.value)">
    <span class="val mono" id="scL">--</span></div>
  <div class="fila"><label>Hueco</label>
    <input type="range" id="sq" min="20" max="600" step="10"
      oninput="toco();lbl('sqL',this.value+' ms')" onchange="cmd('Z,q,'+this.value,'/snd/p?k=q&v='+this.value)">
    <span class="val mono" id="sqL">--</span></div>
  <div class="fila"><label>Ráfagas</label>
    <input type="range" id="sn" min="1" max="8" step="1"
      oninput="toco();lbl('snL',this.value)" onchange="cmd('Z,n,'+this.value,'/snd/p?k=n&v='+this.value)">
    <span class="val mono" id="snL">--</span></div>
  <div class="fila"><label>Pausa frase</label>
    <input type="range" id="sF" min="100" max="4000" step="50"
      oninput="toco();lbl('sFL',this.value+' ms')" onchange="cmd('Z,F,'+this.value,'/snd/p?k=F&v='+this.value)">
    <span class="val mono" id="sFL">--</span></div>

  <div class="nota">Un escarabajo no emite un tono: raspa un rascador contra una lima de dientes, y cada diente es un chasquido. Los <b>pulsos</b> son cuántos chasquidos por segundo, y esa es la capa que decide si suena a insecto o a alarma de microondas: bajo 30 se oyen separados, sobre 120 se funden en zumbido. La <b>portadora</b> conviene dejarla entre 2 y 4 kHz, que es donde el piezo suena fuerte de verdad. La <b>pausa de frase</b> solo se aplica en modo continuo.</div>
</div>

<div class="card">
  <h2>Ojos</h2>
  <div class="fila"><label>Ritmo</label>
    <input type="range" id="ob" min="20" max="220" step="1"
      oninput="toco();lbl('obL',this.value+' bpm')" onchange="cmd('Y,B,'+this.value,'/ojos/p?k=B&v='+this.value)">
    <span class="val mono" id="obL">--</span></div>
  <div class="fila"><label>Brillo base</label>
    <input type="range" id="os" min="0" max="60" step="1"
      oninput="toco();lbl('osL',this.value+' %')" onchange="cmd('Y,S,'+this.value,'/ojos/p?k=S&v='+this.value)">
    <span class="val mono" id="osL">--</span></div>
  <div class="fila"><label>Brillo pico</label>
    <input type="range" id="op" min="10" max="100" step="1"
      oninput="toco();lbl('opL',this.value+' %')" onchange="cmd('Y,P,'+this.value,'/ojos/p?k=P&v='+this.value)">
    <span class="val mono" id="opL">--</span></div>
  <div class="sw">
    <label><input type="checkbox" id="oa" onchange="cmd('Y,A,'+(this.checked?1:0),'/ojos/p?k=A&v='+(this.checked?1:0))">Ojos alternados</label>
    <label><input type="checkbox" id="om" onchange="cmd('Y,M,'+(this.checked?1:0),'/ojos/p?k=M&v='+(this.checked?1:0))">Latir con la marcha</label>
  </div>
  <div class="nota">La curva tiene dos picos, fuerte y suave, como un lub-dub, con corrección de gamma para que el desvanecido se vea parejo. Con "latir con la marcha" el corazón se acelera al caminar: un latido por cada paso.</div>
</div>

<div class="card" id="medidor" data-on="0">
  <h2>Límite de cada servo</h2>

  <div id="panelLim">
    <div id="lims"></div>
    <div class="nota">Cada servo tiene su propia velocidad máxima, porque cargan cosas distintas. El firmware usa el más exigido para decidir hasta dónde puede acortar el ciclo. Si nunca se mide, quedan las estimaciones de fábrica.</div>
  </div>

  <div id="panelMed" style="display:none">
    <div style="text-align:center;padding:6px 0 12px">
      <div class="ts" id="medQuien" style="letter-spacing:.1em">--</div>
      <div class="mono" id="medVal" style="font-size:2.1rem;font-weight:500;margin:6px 0">--</div>
      <div class="ts" id="medT">--</div>
    </div>
    <div class="grid2">
      <button class="btn" onclick="cmd('j','/cancelar')">Cancelar</button>
      <button class="btn guardar" onclick="cmd('h','/marcar');toast('Límite guardado')">Marcar aquí</button>
    </div>
    <div class="nota">El servo va acelerando solo. Observa un único servo y marca en cuanto deje de llegar a sus dos extremos, empiece a temblar o suene forzado. Se guarda el 85 % de ese valor como margen.</div>
  </div>
</div>

<div class="card">
  <details><summary>Ajustes finos</summary>
    <div class="sw" style="margin-top:10px">
      <label><input type="checkbox" id="sil" onchange="mandarCfg()">Silencio en reposo</label>
    </div>
    <div class="nota">Corta el pulso tras 1,5 s quieto para que el servo deje de zumbar.</div>

    <div class="campos" style="grid-template-columns:1fr 1fr;margin-top:14px">
      <div><span class="k">OSCILOSCOPIO CH1 · GPIO5</span>
        <select id="ma" onchange="mandarCfg()">
          <option value="0">Pata izquierda</option><option value="1">Pata derecha</option><option value="2">Balancín</option>
        </select></div>
      <div><span class="k">OSCILOSCOPIO CH2 · GPIO6</span>
        <select id="mb" onchange="mandarCfg()">
          <option value="0">Pata izquierda</option><option value="1">Pata derecha</option><option value="2">Balancín</option>
        </select></div>
    </div>
    <div class="nota">GPIO5 y GPIO6 entregan el ángulo de cada servo como PWM de 20 kHz; con un filtro RC de 4,7 kΩ y 1 µF se convierten en una tensión que dibuja la onda de marcha. GPIO7 da un pulso por ciclo para disparar el osciloscopio, y va directo a la sonda sin filtro. La escala de cada canal se ajusta sola al recorrido del servo.</div>
  </details>
</div>

</div>

<div id="toast" class="toast"></div>

<script>
const NOM=["Pata izquierda","Pata derecha","Balancín"];
const COL=["--izq","--der","--bal"];
const VEL=["Sigiloso","Lento","Normal","Rápido","Muy rápido"];
let st=null, tocado=0, ws=null, apretado=null;

/* ---------- transporte ---------- */
function abrirWS(){
  try{ ws=new WebSocket('ws://'+location.hostname+':81/'); }catch(e){ return; }
  ws.onopen=()=>ping();
  ws.onclose=()=>{ ws=null; ID('lat').textContent='HTTP'; setTimeout(abrirWS,1500); };
  ws.onerror=()=>{ try{ws.close()}catch(e){} };
  ws.onmessage=e=>{
    if(e.data[0]==='K'){ ID('lat').textContent=Math.round(performance.now()-parseFloat(e.data.slice(2)))+' ms'; return; }
    try{ pintar(JSON.parse(e.data)); }catch(err){}
  };
}
function vivo(){ return ws && ws.readyState===1; }
function cmd(c,url){ if(vivo()) ws.send(c); else if(url) fetch(url); }
function ping(){ if(vivo()) ws.send('k,'+performance.now()); }
setInterval(ping,1000);
setInterval(()=>{ if(!vivo()) fetch('/estado').then(r=>r.json()).then(pintar).catch(()=>{}); },300);

/* ---------- helpers ---------- */
const el=(p,i)=>document.getElementById(p+i);
const ID=id=>document.getElementById(id);
function toco(){ tocado=Date.now(); }
function pon(e,v){ if(e && e!==document.activeElement) e.value=v; }
function chk(e,v){ if(e && e!==document.activeElement) e.checked=v; }
function lbl(id,t){ ID(id).textContent=t; }
function toast(m){ const t=ID('toast'); t.textContent=m; t.classList.add('ver');
  clearTimeout(t._h); t._h=setTimeout(()=>t.classList.remove('ver'),1400); }

/* ---------- vistas ---------- */
function vista(v){
  soltar();
  document.body.dataset.v=v;
  cmd('t,'+(v==='manejo'?1:0), '/modo?c='+(v==='manejo'?1:0));
}

/* ---------- cruceta ---------- */
function pisar(b){
  if(!b || b.disabled || apretado===b) return;
  soltar();
  apretado=b; b.classList.add('on');
  cmd('n,'+b.dataset.d, '/conducir?id='+b.dataset.d);
}
function soltar(){
  if(!apretado) return;
  apretado.classList.remove('on'); apretado=null;
  cmd('x','/soltar');
}
ID('pad').addEventListener('pointerdown',e=>{
  const b=e.target.closest('.dir'); if(!b) return;
  e.preventDefault(); if(b.setPointerCapture) b.setPointerCapture(e.pointerId); pisar(b);
});
ID('pad').addEventListener('pointerup',soltar);
ID('pad').addEventListener('pointercancel',soltar);
window.addEventListener('pointerup',soltar);
window.addEventListener('blur',soltar);
document.addEventListener('visibilitychange',()=>{ if(document.hidden) soltar(); });

const TECLA={ArrowUp:'fw',ArrowDown:'bk',ArrowLeft:'lf',ArrowRight:'rt'};
addEventListener('keydown',e=>{ if(document.body.dataset.v!=='manejo'||e.repeat) return;
  const d=TECLA[e.key]; if(!d) return; e.preventDefault();
  pisar(document.querySelector('.dir[data-d="'+d+'"]')); });
addEventListener('keyup',e=>{ if(TECLA[e.key]) soltar(); });

ID('vels').addEventListener('click',e=>{
  const b=e.target.closest('button'); if(!b) return;
  cmd('l,'+b.dataset.v, '/nivel?n='+b.dataset.v);
});

/* ---------- sliders con ritmo de pantalla ---------- */
let cola={}, raf=0;
function encolar(i,a){ toco(); cola[i]=a; if(!raf) raf=requestAnimationFrame(vaciar); }
function vaciar(){ raf=0; for(const i in cola) cmd('s,'+i+','+cola[i],'/set?s='+i+'&a='+cola[i]); cola={}; }

/* ---------- controles de configuración ---------- */
const cal=ID('cal'), sv=ID('servos');
NOM.forEach((n,i)=>{
  cal.insertAdjacentHTML('beforeend',
    '<div class="fila"><label>'+n+'</label>'+
    '<input type="range" id="c'+i+'" min="0" max="180" step="1" '+
    'oninput="lbl(\'cv'+i+'\',this.value+String.fromCharCode(176));encolar('+i+',this.value)">'+
    '<span class="val mono" id="cv'+i+'">--</span></div>');
  sv.insertAdjacentHTML('beforeend',
    '<div class="servo s'+i+'"><h3>'+n+'<span class="ang mono" id="ang'+i+'">--</span></h3>'+
    '<div class="campos">'+
      '<div><span class="k">DESDE</span><input type="number" id="mn'+i+'" min="0" max="180" onfocus="toco()" oninput="toco()" onchange="setOsc('+i+')"></div>'+
      '<div><span class="k">HASTA</span><input type="number" id="mx'+i+'" min="0" max="180" onfocus="toco()" oninput="toco()" onchange="setOsc('+i+')"></div>'+
      '<div><span class="k">FASE</span><input type="number" id="fs'+i+'" min="0" max="360" step="5" onfocus="toco()" oninput="toco()" onchange="setOsc('+i+')"></div>'+
      '<div><span class="k">FORMA</span><select id="fm'+i+'" onchange="setOsc('+i+')">'+
        '<option value="0">Balanceo</option><option value="1">Empuje</option></select></div>'+
    '</div><div class="sw">'+
      '<label><input type="checkbox" id="on'+i+'" onchange="setOsc('+i+')">Activo</label>'+
      '<label><input type="checkbox" id="iv'+i+'" onchange="setOsc('+i+')">Invertir</label>'+
    '</div></div>');
});

function todos(a){ for(let i=0;i<3;i++){ el('c',i).value=a; lbl('cv'+i,a+'\u00B0'); encolar(i,a);} }
function setOsc(i){ toco();
  const mn=el('mn',i).value, mx=el('mx',i).value, fs=el('fs',i).value,
        on=el('on',i).checked?1:0, iv=el('iv',i).checked?1:0, fm=el('fm',i).value;
  cmd('o,'+i+','+mn+','+mx+','+fs+','+on+','+iv+','+fm,
      '/osc?s='+i+'&min='+mn+'&max='+mx+'&fase='+fs+'&on='+on+'&inv='+iv+'&fm='+fm); }
const lims=ID('lims');
NOM.forEach((n,i)=>{
  lims.insertAdjacentHTML('beforeend',
    '<div class="fila"><label>'+n+'</label>'+
    '<input type="number" id="vm'+i+'" min="60" max="1200" step="10" style="width:86px;flex:none" '+
    'onfocus="toco()" oninput="toco()" onchange="cmd(\'w,'+i+',\'+this.value,\'/velmax?s='+i+'&v=\'+this.value)">'+
    '<span class="ts" style="min-width:34px">\u00B0/s</span>'+
    '<button class="btn" style="padding:7px 11px" onclick="cmd(\'e,'+i+'\',\'/medir?s='+i+'\')">Medir</button>'+
    '</div>');
});

function mandarCfg(){ toco();
  const s=ID('sil').checked?1:0, a=ID('ma').value, b=ID('mb').value;
  cmd('c,'+s+','+a+','+b, '/cfg?sil='+s+'&ma='+a+'&mb='+b); }
function toggleMarcha(){ const on=(st&&st.c)?0:1; cmd('m,'+on,'/marcha?on='+on); }
function pre(id){ cmd('p,g,'+id,'/preset/guardar?id='+id); toast('Rutina guardada'); tocado=0; }

/* ---------- misma matemática que el firmware ---------- */
function ss(u){ u=u<0?0:(u>1?1:u); return u*u*(3-2*u); }
function trap(t){ const e=.25, V=1/(1-e);
  if(t<=0) return 0; if(t>=1) return 1;
  if(t<e) return V*t*t/(2*e);
  if(t<1-e) return V*e/2+V*(t-e);
  const u=1-t; return 1-V*u*u/(2*e); }
function conPausa(t,p,suave){ p=Math.min(.8,Math.max(0,p)); const b=p/2;
  if(t<=b) return 0; if(t>=1-b) return 1;
  const u=(t-b)/(1-p); return suave?ss(u):trap(u); }
function ondaServo(f,pausa,forma){ f=f-Math.floor(f);
  const primera=f<.5, t=(primera?f:f-.5)*2;
  if(forma==0){ const v=conPausa(t,pausa,true); return primera?v:1-v; }
  return primera ? conPausa(t,pausa,false) : 1-conPausa(t,pausa,true); }

/* ---------- gráfica del ciclo ---------- */
const cv=ID('onda'), cx=cv.getContext('2d');
function dibujar(){
  requestAnimationFrame(dibujar);
  if(document.body.dataset.v!=='config') return;
  const dpr=devicePixelRatio||1, W=cv.clientWidth, H=cv.clientHeight;
  if(!W||!H) return;
  if(cv.width!==Math.round(W*dpr)){ cv.width=Math.round(W*dpr); cv.height=Math.round(H*dpr); }
  cx.setTransform(dpr,0,0,dpr,0,0); cx.clearRect(0,0,W,H);
  const cs=getComputedStyle(document.documentElement), col=n=>cs.getPropertyValue(n).trim();
  cx.strokeStyle=col('--linea'); cx.lineWidth=1; cx.globalAlpha=.5;
  for(let k=0;k<=4;k++){ const x=W*k/4; cx.beginPath(); cx.moveTo(x,0); cx.lineTo(x,H); cx.stroke(); }
  cx.globalAlpha=1;
  if(!st) return;
  const pau=st.pa, amp=st.ae, TOP=7;

  /* franja: qué lado queda apoyado, según el balancín */
  const b=st.osc[2];
  if(b && b.on){
    for(let px=0;px<W;px++){
      let w=ondaServo(px/W + b.fs/360, pau, b.fm); if(b.iv) w=1-w;
      cx.fillStyle=col('--bal'); cx.globalAlpha = w>.5 ? .55 : .12;
      cx.fillRect(px,0,1,TOP-2);
    }
    cx.globalAlpha=1;
  }

  /* autoescala vertical al rango realmente usado */
  let lo=999, hi=-999;
  for(let i=0;i<3;i++){ const o=st.osc[i]; if(!o.on) continue;
    const c=(o.mn+o.mx)/2, a=(o.mx-o.mn)/2*amp;
    lo=Math.min(lo,c-a,st.out[i]); hi=Math.max(hi,c+a,st.out[i]); }
  if(hi-lo<1){ lo=0; hi=180; }
  const m=(hi-lo)*.12; lo-=m; hi+=m;
  const Y=v=>H-6-((v-lo)/(hi-lo))*(H-6-TOP);

  for(let i=0;i<3;i++){
    const o=st.osc[i]; if(!o.on) continue;
    const c=(o.mn+o.mx)/2, a=(o.mx-o.mn)/2*amp;
    cx.strokeStyle=col(COL[i]); cx.lineWidth=2; cx.beginPath();
    for(let px=0;px<=W;px++){
      let w=ondaServo(px/W + o.fs/360, pau, o.fm); if(o.iv) w=1-w;
      const y=Y(c+a*(2*w-1)); px?cx.lineTo(px,y):cx.moveTo(px,y);
    }
    cx.stroke();
  }
  const x=st.f*W;
  cx.strokeStyle=col('--tx'); cx.globalAlpha=st.c?.7:.18; cx.lineWidth=1.5;
  cx.beginPath(); cx.moveTo(x,TOP); cx.lineTo(x,H); cx.stroke(); cx.globalAlpha=1;
  for(let i=0;i<3;i++){ if(!st.osc[i].on) continue;
    cx.fillStyle=col(COL[i]); cx.beginPath(); cx.arc(x,Y(st.out[i]),3.5,0,6.284); cx.fill(); }
}
requestAnimationFrame(dibujar);

/* ---------- pintar estado ---------- */
let faseAnt=0;
function pintar(s){
  st=s;
  ID('nEstado').textContent=s.c?'EN MARCHA':'REPOSO';
  ID('nVel').innerHTML=VEL[s.nv]+' &middot; <span style="color:'+(s.lim?'var(--alerta)':'var(--mut)')+'">'
    +Math.round(s.pe)+' ms</span>';
  [...ID('vels').children].forEach((b,k)=>b.classList.toggle('sel',k===s.nv));
  document.querySelectorAll('.dir').forEach(b=>{
    b.disabled=!(s.pr & (1<<({fw:0,bk:1,lf:2,rt:3})[b.dataset.d]));
  });
  ID('pista').textContent = s.pr!==15
    ? 'Las flechas apagadas no tienen rutina guardada. Asígnalas desde Configurar.'
    : s.lim
      ? 'Este nivel pide un ciclo más corto del que los servos pueden seguir, así que se limitó.'
      : 'Mantén presionada una flecha para caminar. Al soltarla vuelve a reposo.';
  if(s.c && s.f<faseAnt){ const p=ID('pulso');
    p.animate([{opacity:.20,transform:'scale(.2)'},{opacity:0,transform:'scale(1)'}],{duration:380}); }
  faseAnt=s.f;

  ID('tgSnd').className = s.sOn ? 'btn ir' : 'btn';
  ID('tgOjo').className = s.oOn ? 'btn ir' : 'btn';

  // --- asistente de medición ---
  const midiendo = s.cs >= 0;
  ID('panelLim').style.display = midiendo ? 'none' : '';
  ID('panelMed').style.display = midiendo ? '' : 'none';
  if(midiendo){
    ID('medQuien').textContent = NOM[s.cs].toUpperCase();
    ID('medVal').textContent = s.cvp + ' °/s';
    ID('medT').textContent = 'barrido de ' + s.ct + ' ms';
  }

  const chip=ID('chip'), b=ID('btnMarcha');
  chip.className='chip '+(s.c?'run':'cal'); chip.textContent=s.c?'CAMINANDO':'REPOSO';
  b.textContent=s.c?'Detener':'Iniciar marcha'; b.className=s.c?'btn parar':'btn ir';
  for(let i=0;i<3;i++){
    const o=s.osc[i], rec=(o.mx-o.mn)*s.ae;
    const vp=Math.round(3*rec/Math.max(.05,(1-s.pa))/(s.pe/1000));
    el('ang',i).innerHTML=s.out[i].toFixed(1)+'\u00B0 <b style="font-weight:600;color:'+
      (vp>s.vm[i]?'var(--alerta)':'var(--mut)')+'">'+vp+' \u00B0/s</b>';
  }
  if(Date.now()-tocado<2500) return;
  for(let i=0;i<3;i++){
    pon(el('c',i),Math.round(s.cal[i])); lbl('cv'+i,Math.round(s.cal[i])+'\u00B0');
    pon(el('mn',i),s.osc[i].mn); pon(el('mx',i),s.osc[i].mx); pon(el('fs',i),s.osc[i].fs);
    pon(el('fm',i),s.osc[i].fm); pon(el('vm',i),s.vm[i]);
    chk(el('on',i),!!s.osc[i].on); chk(el('iv',i),!!s.osc[i].iv);
  }
  pon(ID('per'),s.p); lbl('perL',s.p+' ms');
  pon(ID('amp'),Math.round(s.a*100)); lbl('ampL',Math.round(s.a*100)+' %');
  pon(ID('pau'),Math.round(s.pa*100)); lbl('pauL',Math.round(s.pa*100)+' %');
  chk(ID('sil'),!!s.sil);
  pon(ID('ma'),s.ma); pon(ID('mb'),s.mb);

  pon(ID('sm'),s.sMo);
  const SS=[['su','sPu',''],['sd','sDu',' %'],['sf','sPo',' Hz'],['sv','sVa',' %'],
            ['sb','sBa',' %'],['sw','sVo',' %'],['sc','sCh',' ms'],['sq','sGa',' ms'],
            ['sn','sNc',''],['sF','sFr',' ms'],['ob','oBp',' bpm'],['os','oBa',' %'],
            ['op','oPi',' %']];
  SS.forEach(([id,k,suf])=>{ pon(ID(id),s[k]); lbl(id+'L',s[k]+suf); });
  chk(ID('oa'),!!s.oAl); chk(ID('om'),!!s.oMa);
}
abrirWS();
</script></body></html>
)HTML";

// ==================================================
//  ESTADO EN JSON
// ==================================================
String estadoJSON() {
  String j = "{\"c\":" + String(caminando ? 1 : 0);
  j += ",\"md\":" + String(modoConduccion ? 1 : 0);
  j += ",\"nv\":" + String(nivelVel);
  j += ",\"pr\":" + String(presetsBit);
  j += ",\"f\":"  + String(faseCiclo, 3);
  j += ",\"p\":"  + String(marcha.periodo);
  j += ",\"pe\":" + String((int)Tef);
  j += ",\"lim\":" + String(limitado ? 1 : 0);
  j += ",\"pa\":" + String(marcha.pausa, 2);
  j += ",\"a\":"  + String(marcha.amplitud, 2);
  j += ",\"ae\":" + String(Aef, 2);
  j += ",\"sil\":" + String(cfg.silencio ? 1 : 0);
  j += ",\"ma\":" + String(cfg.monA);
  j += ",\"mb\":" + String(cfg.monB);
  j += ",\"sOn\":" + String(snd.activo ? 1 : 0);
  j += ",\"sMo\":" + String(snd.modo);
  j += ",\"sPu\":" + String(snd.pulsoHz);
  j += ",\"sDu\":" + String(snd.dutyPct);
  j += ",\"sPo\":" + String(snd.portadora);
  j += ",\"sVa\":" + String(snd.varPct);
  j += ",\"sBa\":" + String(snd.barridoPct);
  j += ",\"sVo\":" + String(snd.volPct);
  j += ",\"sCh\":" + String(snd.chirpMs);
  j += ",\"sGa\":" + String(snd.gapMs);
  j += ",\"sNc\":" + String(snd.nChirps);
  j += ",\"sFr\":" + String(snd.fraseMs);
  j += ",\"oOn\":" + String(ojos.activo ? 1 : 0);
  j += ",\"oBp\":" + String(ojos.bpm);
  j += ",\"oBa\":" + String(ojos.basePct);
  j += ",\"oPi\":" + String(ojos.picoPct);
  j += ",\"oAl\":" + String(ojos.alterno ? 1 : 0);
  j += ",\"oMa\":" + String(ojos.conMarcha ? 1 : 0);
  j += ",\"cs\":" + String(calServo);
  j += ",\"cvp\":" + String((int)calVelocidadPedida());
  j += ",\"ct\":" + String((int)calT);
  j += ",\"vm\":[";
  for (int i = 0; i < N_SERVOS; i++) j += String((int)cfg.velMax[i]) + (i < N_SERVOS - 1 ? "," : "]");
  j += ",\"cal\":[";
  for (int i = 0; i < N_SERVOS; i++) j += String(anguloActual[i], 1) + (i < N_SERVOS - 1 ? "," : "]");
  j += ",\"out\":[";
  for (int i = 0; i < N_SERVOS; i++) j += String(salida[i], 1) + (i < N_SERVOS - 1 ? "," : "]");
  j += ",\"osc\":[";
  for (int i = 0; i < N_SERVOS; i++) {
    OscParam &o = marcha.osc[i];
    j += "{\"mn\":" + String(o.angMin, 0) + ",\"mx\":" + String(o.angMax, 0) +
         ",\"fs\":" + String(o.fase, 0) + ",\"on\":" + (o.activo ? 1 : 0) +
         ",\"iv\":" + (o.invertir ? 1 : 0) + ",\"fm\":" + String(o.forma) + "}";
    if (i < N_SERVOS - 1) j += ",";
  }
  j += "]}";
  return j;
}

/* La librería WebSockets pide String& (referencia no-const): el JSON
   tiene que vivir en una variable con nombre, no ser un temporal. */
void enviarEstado(int cliente = -1) {
  String j = estadoJSON();
  if (cliente < 0) ws.broadcastTXT(j);
  else             ws.sendTXT((uint8_t)cliente, j);
}

// ==================================================
//  WEBSOCKET
// ==================================================
String campo(const String &s, int n) {
  int ini = 0, idx = 0;
  while (idx < n) {
    ini = s.indexOf(',', ini);
    if (ini < 0) return "";
    ini++; idx++;
  }
  int fin = s.indexOf(',', ini);
  return fin < 0 ? s.substring(ini) : s.substring(ini, fin);
}

void onWS(uint8_t cliente, WStype_t tipo, uint8_t *payload, size_t len) {
  if (tipo == WStype_CONNECTED) { enviarEstado(cliente); return; }
  if (tipo == WStype_DISCONNECTED) {
    if (ws.connectedClients() == 0 && modoConduccion) aSoltar();
    return;
  }
  if (tipo != WStype_TEXT) return;

  String m = String((char*)payload).substring(0, len);
  if (m.length() == 0) return;

  switch (m[0]) {
    case 'k': { String pong = "K," + campo(m, 1);
                ws.sendTXT(cliente, pong); return; }
    case 'n': aConducir(campo(m, 1)); break;
    case 'x': aSoltar(); break;
    case 't': aModo(campo(m, 1).toInt() == 1); break;
    case 'l': aNivel(campo(m, 1).toInt()); break;
    case 's': aSet(campo(m,1).toInt(), campo(m,2).toFloat()); break;
    case 'o': aOsc(campo(m,1).toInt(), campo(m,2).toFloat(), campo(m,3).toFloat(),
                   campo(m,4).toFloat(), campo(m,5).toInt() == 1, campo(m,6).toInt() == 1,
                   campo(m,7).toInt()); break;
    case 'v': aVel(campo(m,1).toInt()); break;
    case 'a': aAmp(campo(m,1).toFloat()); break;
    case 'u': aPausa(campo(m,1).toFloat()); break;
    case 'c': aCfg(campo(m,1).toInt() == 1, campo(m,2).toInt(), campo(m,3).toInt()); break;
    case 'w': aVelMax(campo(m,1).toInt(), campo(m,2).toFloat()); break;
    case 'e': aMedir(campo(m,1).toInt()); break;
    case 'h': aMarcar(); break;
    case 'j': aCancelarMedicion(); break;
    case 'm': if (campo(m,1).toInt() == 1) { despertar(); caminando = true; }
              else caminando = false; break;
    case 'r': irReposo(); break;
    case 'g': for (int i = 0; i < N_SERVOS; i++) anguloReposo[i] = anguloActual[i];
              guardarReposo(); break;
    case 'p': aPreset(campo(m,1)[0], campo(m,2)); break;
    /* Los comandos de sonido y ojos usan mayúsculas y prefijo, porque las
       minúsculas ya estaban tomadas por los comandos de marcha. */
    case 'S': aSndActivo(campo(m,1).toInt() == 1); break;
    case 'M': aSndModo(campo(m,1).toInt()); break;
    case 'P': aSndPreset(campo(m,1).toInt()); break;
    case 'T': aSndProbar(); break;
    case 'Z': aSndParam(campo(m,1)[0], campo(m,2).toInt()); break;
    case 'O': aOjosActivo(campo(m,1).toInt() == 1); break;
    case 'Y': aOjosParam(campo(m,1)[0], campo(m,2).toInt()); break;
  }
  enviarEstado();
}

// ==================================================
//  HTTP (respaldo si el WebSocket falla)
// ==================================================
void hOK()     { server.send(200, "application/json", "{\"ok\":1}"); }
void hRoot()   { server.send_P(200, "text/html", PAGINA); }
void hEstado() { server.send(200, "application/json", estadoJSON()); }

// ==================================================
//  SETUP
// ==================================================
void setup() {
  Serial.begin(115200);
  delay(300);

  cargarTodo();

  // Solo dos timers para los servos: los tres van a 50 Hz y comparten uno.
  // Los timers restantes quedan libres para el PWM de monitoreo.
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  for (int i = 0; i < N_SERVOS; i++) {
    engancharServo(i, true);
    escribirServo(i, anguloReposo[i]);
  }
#if MONITOR_OSC
  iniciarMonitor();
#endif

  randomSeed(esp_random());
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_BUZZER, snd.portadora, PWM_BITS);
#else
  ledcSetup(CH_BUZZER, snd.portadora, PWM_BITS);
  ledcAttachPin(PIN_BUZZER, CH_BUZZER);
#endif
  tonoOff();
  if (ojos.activo) engancharLeds(true);
  if (snd.activo && snd.modo == 0) dispararFrase();

  tQuieto = millis();

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);
  WiFi.softAP(AP_SSID, AP_PASS, AP_CANAL, 0, AP_MAX_CLIENTES);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  dns.start(53, "*", AP_IP);

  Serial.println();
  Serial.print("Red: ");     Serial.print(AP_SSID);
  Serial.print("  clave: "); Serial.println(AP_PASS);
  Serial.print("Abrir: http://"); Serial.println(WiFi.softAPIP());
  Serial.printf("Servos -> izq: GPIO%d  der: GPIO%d  balancin: GPIO%d\n",
                PIN_PATA_IZQ, PIN_PATA_DER, PIN_BALANCIN);

  server.on("/", hRoot);
  server.on("/estado", hEstado);
  server.on("/ping", []() { server.send(200, "text/plain", "pong"); });
  server.on("/conducir", []() { aConducir(server.arg("id")); hOK(); });
  server.on("/soltar",   []() { aSoltar(); hOK(); });
  server.on("/modo",     []() { aModo(server.arg("c").toInt() == 1); hOK(); });
  server.on("/nivel",    []() { aNivel(server.arg("n").toInt()); hOK(); });
  server.on("/set", []() { aSet(server.arg("s").toInt(), server.arg("a").toFloat()); hOK(); });
  server.on("/osc", []() { aOsc(server.arg("s").toInt(), server.arg("min").toFloat(),
                                server.arg("max").toFloat(), server.arg("fase").toFloat(),
                                server.arg("on").toInt() == 1, server.arg("inv").toInt() == 1,
                                server.arg("fm").toInt()); hOK(); });
  server.on("/vel",   []() { aVel(server.arg("T").toInt()); hOK(); });
  server.on("/amp",   []() { aAmp(server.arg("v").toFloat()); hOK(); });
  server.on("/pausa", []() { aPausa(server.arg("v").toFloat()); hOK(); });
  server.on("/cfg",   []() { aCfg(server.arg("sil").toInt() == 1,
                                  server.arg("ma").toInt(), server.arg("mb").toInt()); hOK(); });
  server.on("/velmax", []() { aVelMax(server.arg("s").toInt(), server.arg("v").toFloat()); hOK(); });
  server.on("/medir",  []() { aMedir(server.arg("s").toInt()); hOK(); });
  server.on("/marcar", []() { aMarcar(); hOK(); });
  server.on("/cancelar", []() { aCancelarMedicion(); hOK(); });
  server.on("/marcha", []() {
    if (server.arg("on").toInt() == 1) { despertar(); caminando = true; } else caminando = false;
    hOK();
  });
  server.on("/reposo/ir", []() { irReposo(); hOK(); });
  server.on("/reposo/guardar", []() {
    for (int i = 0; i < N_SERVOS; i++) anguloReposo[i] = anguloActual[i];
    guardarReposo(); hOK();
  });
  server.on("/preset/guardar", []() { aPreset('g', server.arg("id")); hOK(); });
  server.on("/preset/cargar",  []() { aPreset('c', server.arg("id")); hOK(); });
  server.on("/snd/on",     []() { aSndActivo(server.arg("v").toInt() == 1); hOK(); });
  server.on("/snd/modo",   []() { aSndModo(server.arg("m").toInt()); hOK(); });
  server.on("/snd/preset", []() { aSndPreset(server.arg("i").toInt()); hOK(); });
  server.on("/snd/probar", []() { aSndProbar(); hOK(); });
  server.on("/snd/p",      []() { aSndParam(server.arg("k")[0], server.arg("v").toInt()); hOK(); });
  server.on("/ojos/on",    []() { aOjosActivo(server.arg("v").toInt() == 1); hOK(); });
  server.on("/ojos/p",     []() { aOjosParam(server.arg("k")[0], server.arg("v").toInt()); hOK(); });
  server.onNotFound(hRoot);
  server.begin();

  ws.begin();
  ws.onEvent(onWS);
}

// ==================================================
//  LOOP
// ==================================================
void loop() {
  dns.processNextRequest();
  server.handleClient();
  ws.loop();

  // El sonido se actualiza en cada vuelta: la gatilla de pulsos necesita
  // resolución de milisegundo y no puede esperar al ciclo de 50 Hz.
  actualizarSonido();

  static uint32_t tPrev = 0;
  uint32_t ahora = micros();
  if (ahora - tPrev >= 20000UL) {          // 50 Hz, alineado con la trama del servo
    float dt = (ahora - tPrev) / 1000000.0f;
    tPrev = ahora;
    if (dt > 0.1f) dt = 0.1f;
    actualizarMovimiento(dt);
    actualizarOjos(dt);
  }

  static uint32_t tTel = 0;
  uint32_t intervalo = (caminando || mezcla > 0.001f) ? 60 : 300;
  if (millis() - tTel >= intervalo) {
    tTel = millis();
    if (ws.connectedClients() > 0) enviarEstado();
  }
}
