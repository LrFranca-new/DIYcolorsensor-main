#include <Arduino.h>
#include "DIYcolorsensor.h"

// ---------------------------------------------------------------------------
//  Funções auxiliares (visíveis só neste arquivo)
// ---------------------------------------------------------------------------
namespace {

// Uma leitura do ADC junto com o instante (µs) em que ela foi disparada.
struct Sample {
  uint16_t v;
  uint32_t t;
};

inline Sample takeSample(byte pin) {
  Sample s;
  s.t = (uint32_t)micros();
  s.v = (uint16_t)analogRead(pin);
  return s;
}

// Luz ambiente estimada no instante 't', por interpolação linear entre duas
// leituras com o LED apagado: 'a' (antes do pulso) e 'b' (depois dele).
//
//   ambiente(t) = a.v + (b.v - a.v) * (t - a.t) / (b.t - a.t)
//
// Os instantes vêm do micros(), então o peso continua certo mesmo se
// rise != fall ou se uma interrupção (Serial, timer...) atrasar um delay.
// Só aritmética inteira (rápida no AVR) e o resultado nunca sai de [a.v, b.v].
int32_t ambientAt(const Sample &a, const Sample &b, uint32_t t) {
  const int32_t span = (int32_t)(b.t - a.t);
  if (span <= 0) return ((int32_t)a.v + (int32_t)b.v) / 2;   // sem informação de tempo: média simples

  int32_t offset = (int32_t)(t - a.t);
  if (offset < 0) offset = 0;
  if (offset > span) offset = span;

  const int32_t num  = ((int32_t)b.v - (int32_t)a.v) * offset;
  const int32_t half = span / 2;                              // arredonda para o inteiro mais próximo
  return (int32_t)a.v + (num >= 0 ? num + half : num - half) / span;
}

// Luz refletida = leitura com o LED aceso - ambiente NAQUELE instante.
inline int32_t reflected(const Sample &lit, const Sample &before, const Sample &after) {
  return (int32_t)lit.v - ambientAt(before, after, lit.t);
}

// Mesma regra da 1.6.9: <= margem vira 0; acima dela, subtrai a margem e aplica o ganho.
inline int toRaw(int32_t diff, int margin, float gain) {
  return (diff <= margin) ? 0 : (int)(gain * (float)(diff - margin));
}

}  // namespace

// ---------------------------------------------------------------------------
//  Construtor / inicialização
// ---------------------------------------------------------------------------
ColorSensor::ColorSensor(byte r, byte g, byte b, byte p, float RmS, float GmS, float BmS)
    : r_pin(r), g_pin(g), b_pin(b), pht(p), Rm(RmS), Gm(GmS), Bm(BmS),
      color(COLOR_NONE), tStart(0), tEnd(0) {
  // As faixas começam "desligadas" (0..0): só casariam com R=G=B=0, que já é COLOR_NONE.
  for (byte i = 0; i < 7; i++) {
    for (byte k = 0; k < 3; k++) {
      range[i].minimum[k] = 0;
      range[i].maximum[k] = 0;
    }
  }
  pinMode(r_pin, OUTPUT);
  pinMode(g_pin, OUTPUT);
  pinMode(b_pin, OUTPUT);
  pinMode(pht, INPUT);
}

void ColorSensor::begin() {
  pinMode(r_pin, OUTPUT);
  pinMode(g_pin, OUTPUT);
  pinMode(b_pin, OUTPUT);
  pinMode(pht, INPUT);
  digitalWrite(r_pin, LOW);
  digitalWrite(g_pin, LOW);
  digitalWrite(b_pin, LOW);
}

// ---------------------------------------------------------------------------
//  RGB -> HSB
//  Mesmo resultado da 1.6.9. A entrada é dividida por 255 sem limitar, então
//  brightness pode passar de 1.0 (R_raw/G_raw/B_raw podem passar de 255).
// ---------------------------------------------------------------------------
void ColorSensor::rgbToHSB(uint16_t r, uint16_t g, uint16_t b) {
  const float R = float(r) / 255.0;
  const float G = float(g) / 255.0;
  const float B = float(b) / 255.0;

  const float max_val = (R > G) ? ((R > B) ? R : B) : ((G > B) ? G : B);
  const float min_val = (R < G) ? ((R < B) ? R : B) : ((G < B) ? G : B);
  const float delta = max_val - min_val;

  brightness = max_val;
  saturation = (max_val == 0) ? 0 : delta / max_val;

  hue = 0;
  if (delta != 0) {
    if (max_val == R) {
      hue = 60 * ((G - B) / delta);       // aqui (G-B)/delta já está em [-1, 1]; o fmod(.., 6) da 1.6.9 não fazia nada
    } else if (max_val == G) {
      hue = 60 * ((B - R) / delta + 2);
    } else {
      hue = 60 * ((R - G) / delta + 4);
    }
  }
  if (hue < 0) {
    hue += 360;
  }
}

// ---------------------------------------------------------------------------
//  Medição RGB com rejeição de luz ambiente por interpolação
//
//  Sequência (7 leituras de ADC):
//
//      A0    R     A1    G     A2    B     A3        A = luz ambiente (LEDs apagados)
//      ●─────●─────●─────●─────●─────●─────●         R, G, B = LED daquele canal aceso
//
//  Para cada canal, o ambiente é interpolado entre as duas leituras "A" que o
//  cercam, no instante exato da leitura com o LED aceso:
//
//      refletido(R) = R - ambiente_interpolado(A0, A1, no instante de R)
//
//  Por que isso é melhor que "aceso - apagado depois" (1.6.9):
//   * Lâmpada AC pisca a 120 Hz. Se o ambiente estiver subindo ou descendo
//     durante a janela de ~1 ms, a 1.6.9 interpretava a inclinação como reflexo
//     (o "vazamento" de 20+ pontos que aparecia no log apontado para o nada).
//     Com o ambiente medido dos dois lados, a inclinação é cancelada.
//   * Cada leitura "A" serve aos dois canais vizinhos: 7 leituras no total
//     (a 1.6.9 fazia 6; medir "antes e depois" separado por canal faria 9).
//
//  Limite: a interpolação é linear, então a curvatura da onda de luz continua
//  vazando um resíduo pequeno. Por isso margin_no_color ainda é útil.
// ---------------------------------------------------------------------------
void ColorSensor::getRGB() {
  Sample amb[4];   // luz ambiente (todos os LEDs apagados)
  Sample lit[3];   // leitura com o LED de cada canal aceso

  tStart = micros();

  amb[0] = takeSample(pht);

  digitalWrite(r_pin, HIGH);
  delayMicroseconds(riseTimeR);
  lit[0] = takeSample(pht);
  digitalWrite(r_pin, LOW);
  delayMicroseconds(fallTimeR);
  amb[1] = takeSample(pht);

  digitalWrite(g_pin, HIGH);
  delayMicroseconds(riseTimeG);
  lit[1] = takeSample(pht);
  digitalWrite(g_pin, LOW);
  delayMicroseconds(fallTimeG);
  amb[2] = takeSample(pht);

  digitalWrite(b_pin, HIGH);
  delayMicroseconds(riseTimeB);
  lit[2] = takeSample(pht);
  digitalWrite(b_pin, LOW);
  delayMicroseconds(fallTimeB);
  amb[3] = takeSample(pht);

  // As contas ficam para depois de medir tudo: não interferem na temporização dos pulsos.
  R_raw = toRaw(reflected(lit[0], amb[0], amb[1]), margin_no_color, Rm);
  G_raw = toRaw(reflected(lit[1], amb[1], amb[2]), margin_no_color, Gm);
  B_raw = toRaw(reflected(lit[2], amb[2], amb[3]), margin_no_color, Bm);

  ambientLevel = (int)(((int32_t)amb[0].v + amb[1].v + amb[2].v + amb[3].v) / 4);
  ambientSaturated = false;
  for (byte i = 0; i < 4; i++) {
    if ((int)amb[i].v >= adcSaturationLevel) ambientSaturated = true;
  }

  tEnd = micros();
}

void ColorSensor::getHSB() {
  getRGB();
  rgbToHSB(R_raw, G_raw, B_raw);
}

// ---------------------------------------------------------------------------
//  Classificação por faixas (cubos RGB)
// ---------------------------------------------------------------------------
bool ColorSensor::inRange(byte code) const {
  const Range &rg = range[code - 1];
  return R_raw >= rg.minimum[0] && G_raw >= rg.minimum[1] && B_raw >= rg.minimum[2] &&
         R_raw <= rg.maximum[0] && G_raw <= rg.maximum[1] && B_raw <= rg.maximum[2];
}

// Mesma ordem de prioridade da 1.6.9: preto, verde, vermelho, branco, amarelo, azul, prata.
// Verde e vermelho exigem que o canal seja o dominante.
byte ColorSensor::classify() const {
  if (R_raw <= noColorLimit && G_raw <= noColorLimit && B_raw <= noColorLimit) return COLOR_NONE;
  if (inRange(COLOR_BLACK)) return COLOR_BLACK;
  if (inRange(COLOR_GREEN) && G_raw > R_raw && G_raw > B_raw) return COLOR_GREEN;
  if (inRange(COLOR_RED) && R_raw > G_raw && R_raw > B_raw) return COLOR_RED;
  if (inRange(COLOR_WHITE)) return COLOR_WHITE;
  if (inRange(COLOR_YELLOW)) return COLOR_YELLOW;
  if (inRange(COLOR_BLUE)) return COLOR_BLUE;
  if (inRange(COLOR_SILVER)) return COLOR_SILVER;
  return COLOR_UNKNOWN;
}

byte ColorSensor::getColor() {
  getRGB();
  // Com o ADC saturado pela luz ambiente a medição não vale nada: R_raw = G_raw = B_raw = 0
  // seria lido como "sem cor". Devolve "desconhecida"; ambientSaturated diz o motivo.
  color = ambientSaturated ? (byte)COLOR_UNKNOWN : classify();
  return color;
}

// ---------------------------------------------------------------------------
//  Escuridão (só o LED verde), com a mesma interpolação: ambiente antes e depois.
// ---------------------------------------------------------------------------
int ColorSensor::darkness() {
  const Sample before = takeSample(pht);

  digitalWrite(g_pin, HIGH);
  delayMicroseconds(riseTime);
  const Sample lit = takeSample(pht);
  digitalWrite(g_pin, LOW);
  delayMicroseconds(fallTime);
  const Sample after = takeSample(pht);

  ambientLevel = (int)(((int32_t)before.v + after.v) / 2);
  ambientSaturated = ((int)before.v >= adcSaturationLevel) || ((int)after.v >= adcSaturationLevel);

  const int32_t diff = reflected(lit, before, after);
  return (diff <= margin_no_color) ? 0 : (int)(diff - margin_no_color);
}

bool ColorSensor::isBlack() {
  const int d = darkness();
  return !ambientSaturated && d <= cutOffValue;   // cego pela luz ambiente não é "preto"
}

// ---------------------------------------------------------------------------
//  Calibração
// ---------------------------------------------------------------------------
void ColorSensor::setColorMin(byte code, int r, int g, int b) {
  if (code < COLOR_BLACK || code > COLOR_SILVER) return;   // COLOR_NONE / COLOR_UNKNOWN não têm faixa
  Range &rg = range[code - 1];
  rg.minimum[0] = r;
  rg.minimum[1] = g;
  rg.minimum[2] = b;
}

void ColorSensor::setColorMax(byte code, int r, int g, int b) {
  if (code < COLOR_BLACK || code > COLOR_SILVER) return;
  Range &rg = range[code - 1];
  rg.maximum[0] = r;
  rg.maximum[1] = g;
  rg.maximum[2] = b;
}

// ---------------------------------------------------------------------------
//  Depuração
// ---------------------------------------------------------------------------
void ColorSensor::printValues() {
  unsigned long dt = tEnd - tStart;
  if (dt == 0) dt = 1;                                      // evita divisão por zero

  Serial.print("Red: ");
  Serial.print(R_raw);
  Serial.print(" | Green: ");
  Serial.print(G_raw);
  Serial.print(" | Blue: ");
  Serial.println(B_raw);
  Serial.print("Frequency: ");
  Serial.print(1000000UL / dt);
  Serial.println("Hz");
  Serial.print("Color: ");
  Serial.println(color);
  Serial.print("Ambient: ");
  Serial.print(ambientLevel);
  Serial.println(ambientSaturated ? " (ADC saturado!)" : "");
  Serial.println();
}
