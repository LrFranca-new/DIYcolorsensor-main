/*
  DIYcolorsensor — sensor de cor RGB com LED RGB + fototransistor (ou fotodiodo)
  Versão 1.7.0

  Novidades em relação à 1.6.9
  ----------------------------
  * getRGB() reescrita: a luz ambiente passa a ser INTERPOLADA no instante exato
    em que o LED está aceso (antes ela era lida só depois do pulso). Isso cancela
    a parte "linear" do pisca-pisca de 120 Hz das lâmpadas.
  * darkness() / isBlack() usam a mesma medição com interpolação.
  * Proteção contra saturação do ADC (sol, luz forte): veja ambientSaturated.
  * noColorLimit substitui o "10" fixo que estava dentro de getColor().
  * setColorMin() / setColorMax() unificam os 14 setters (os antigos continuam
    funcionando) e COLOR_* dão nome aos códigos de cor.
  * getHSB() = getRGB() + rgbToHSB().  begin() é opcional.

  Todo o resto da API pública (nomes, tipos e valores padrão) foi mantido, então
  sketches escritos para a 1.6.9 continuam compilando sem alteração.
*/
#ifndef DIYCOLORSENSOR_H
#define DIYCOLORSENSOR_H

#include <Arduino.h>

// Maior valor que o analogRead() da sua placa devolve. Se usar outra resolução,
// defina ANTES do #include:   #define DIYCS_ADC_MAX 4095
#ifndef DIYCS_ADC_MAX
  #if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
    #define DIYCS_ADC_MAX 4095   // ESP32: 12 bits por padrão
  #else
    #define DIYCS_ADC_MAX 1023   // AVR (Uno, Nano...) e a maioria das outras: 10 bits
  #endif
#endif

// Códigos devolvidos por getColor() (mesma tabela do README).
enum ColorCode {
  COLOR_NONE    = 0,   // nada na frente do sensor
  COLOR_BLACK   = 1,
  COLOR_BLUE    = 2,
  COLOR_GREEN   = 3,
  COLOR_YELLOW  = 4,
  COLOR_RED     = 5,
  COLOR_WHITE   = 6,
  COLOR_SILVER  = 7,
  COLOR_UNKNOWN = 8    // não caiu em nenhuma faixa (ou ADC saturado)
};

class ColorSensor {
 public:
  ColorSensor(byte r, byte g, byte b, byte p, float RmS = 1, float GmS = 1, float BmS = 1);

  // Opcional: reconfigura os pinos. Em placas como o ESP32 é boa prática chamar
  // no setup(), porque o construtor roda antes de o Arduino terminar de iniciar.
  void begin();

  // ------------------------------------------------------------------ medição
  void getRGB();                 // mede R_raw, G_raw, B_raw (com rejeição de luz ambiente)
  void getHSB();                 // getRGB() + rgbToHSB(R_raw, G_raw, B_raw)
  byte getColor();               // getRGB() + classificação por faixas (0..8)
  int  darkness();               // reflexo do LED verde (quanto maior, mais claro o objeto)
  bool isBlack();                // darkness() <= cutOffValue (e ADC não saturado)
  void rgbToHSB(uint16_t r, uint16_t g, uint16_t b);
  void printValues();

  // -------------------------------------------------------------- calibração
  // Faixa (cubo RGB) de cada cor: use COLOR_BLACK ... COLOR_SILVER como 'code'.
  void setColorMin(byte code, int r, int g, int b);
  void setColorMax(byte code, int r, int g, int b);

  // Atalhos da versão antiga (chamam setColorMin/setColorMax).
  void setBlackMin(int r, int g, int b)   { setColorMin(COLOR_BLACK,  r, g, b); }
  void setBlackMax(int r, int g, int b)   { setColorMax(COLOR_BLACK,  r, g, b); }
  void setBlueMin(int r, int g, int b)    { setColorMin(COLOR_BLUE,   r, g, b); }
  void setBlueMax(int r, int g, int b)    { setColorMax(COLOR_BLUE,   r, g, b); }
  void setGreenMin(int r, int g, int b)   { setColorMin(COLOR_GREEN,  r, g, b); }
  void setGreenMax(int r, int g, int b)   { setColorMax(COLOR_GREEN,  r, g, b); }
  void setRedMin(int r, int g, int b)     { setColorMin(COLOR_RED,    r, g, b); }
  void setRedMax(int r, int g, int b)     { setColorMax(COLOR_RED,    r, g, b); }
  void setYellowMin(int r, int g, int b)  { setColorMin(COLOR_YELLOW, r, g, b); }
  void setYellowMax(int r, int g, int b)  { setColorMax(COLOR_YELLOW, r, g, b); }
  void setWhiteMin(int r, int g, int b)   { setColorMin(COLOR_WHITE,  r, g, b); }
  void setWhiteMax(int r, int g, int b)   { setColorMax(COLOR_WHITE,  r, g, b); }
  void setSilverMin(int r, int g, int b)  { setColorMin(COLOR_SILVER, r, g, b); }
  void setSilverMax(int r, int g, int b)  { setColorMax(COLOR_SILVER, r, g, b); }

  // ---------------------------------------------------- parâmetros ajustáveis
  bool isArduino = true;               // mantido por compatibilidade

  // Tempos em microssegundos.
  //   riseX: espera depois de ACENDER o LED e antes de ler.
  //   fallX: espera depois de APAGAR o LED e antes de ler a luz ambiente.
  // Dica: com rise == fall o ponto de leitura cai no meio da janela e a
  // interpolação fica mais precisa (mas rise != fall também funciona).
  unsigned long fallTime = 350;        // usados por darkness()
  unsigned long riseTime = 350;
  unsigned long fallTimeR = 350;       // usados por getRGB()
  unsigned long riseTimeR = 350;
  unsigned long fallTimeG = 350;
  unsigned long riseTimeG = 350;
  unsigned long fallTimeB = 350;
  unsigned long riseTimeB = 350;

  int cutOffValue = 150;               // limite de isBlack()
  int margin_no_color = 3;             // reflexo <= margem vira 0; acima dela, a margem é subtraída.
                                       // ATENÇÃO: mudar a margem desloca TODAS as leituras (e as faixas de calibração).
  int noColorLimit = 10;               // getColor(): R, G e B <= isso  ->  COLOR_NONE

  // Se a luz AMBIENTE chegar a esse valor no ADC, o sensor está "cego" (a leitura
  // com o LED aceso não tem mais para onde subir). Padrão: 3 abaixo do fundo de escala.
  int adcSaturationLevel = DIYCS_ADC_MAX - 3;

  // ------------------------------------------- resultado da última medição
  int R_raw = 0, G_raw = 0, B_raw = 0;
  float hue = 0;                       // 0..360
  float saturation = 0;                // 0..1
  float brightness = 0;                // 0..1 (pode passar de 1: R_raw/255)
  int ambientLevel = 0;                // luz ambiente média no ADC (0..DIYCS_ADC_MAX)
  bool ambientSaturated = false;       // true se a luz ambiente saturou o ADC

 private:
  struct Range {
    int minimum[3];
    int maximum[3];
  };

  byte r_pin, g_pin, b_pin, pht;
  float Rm, Gm, Bm;
  byte color;
  unsigned long tStart, tEnd;
  Range range[7];                      // índice = código da cor - 1 (preto ... prata)

  bool inRange(byte code) const;
  byte classify() const;
};

#endif // DIYCOLORSENSOR_H
