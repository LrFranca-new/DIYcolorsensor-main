/*/// Modos de Teste ///////////////////////////////////////////////
intensidade   - O código printará o valor de reflexão da cor verde

frequencia  - Faz leituras de forma contínua de cores e 
              informa quantas vezes cada cor foi detectada;

long        - Entra no modo de longa distância;

nomes       - Printa somente o nome da cor detectada, pode trocar 
              a língua de inglês para português digitando "p"; 

digite "exit" para sair do modo selecionado. 
*/

#include <DIYcolorsensor.h>

ColorSensor sensor(2, 3, 4, A0);

String modoAtual = "";
bool nomes_longReading = false;
bool nomes_pt = false;

byte colorDetection(float h, float s, float b) {
  // 0 = sem cor / 1 = preto / 2 = azul / 3 = verde / 4 = amarelo / 5 = vermelho / 6 = branco / 7 = marrom / 8 = prata

  // Sem leitura válida
  if (b <= 0.12 || s == 0 || s == 1) return 0;

  // Preto: brilho muito baixo OU (brilho baixo E pouca saturação)
  if (b <= 0.30) return 1;
  if (b <= 0.65 && s < 0.45) return 1;
  if (s <= 0.3 && b <= 0.8) return 1;

  // Prata / Branco
  if (s <= 0.1 && b >= 3.0) return 8;
  if (s <= 0.4 && b >= 1.2) return 6;

  // Matiz
  if (h >= 330 || h < 20) return 5;   // Vermelho
  if (h >= 20 && h < 75)  return 4;   // Amarelo
  if (h >= 75 && h < 160) return 3;   // Verde
  if (h >= 160 && h < 270) return 2;  // Azul

  return 6; // Fallback magenta/roxo
}

byte longBnWColorDetection(float h, float s, float b) {
  // Filtros finos baseados no seu código original
  if (b <= 0.12 || s <= 0 || s >= 1) return 0;

    if (h >= 330 || h < 20) return 5;
    if (h >= 20 && h < 75) return 4;
    if (h >= 75 && h < 160) return 3;
    if (h >= 160 && h < 270) return 2;

  return 0;
}

void fixPrint(int v) {
  if (v <= 9) {
    Serial.print("  ");
  } else if (v <= 99) {
    Serial.print(" ");
  }
}

String getMsg() {
  String msg = "";
  if (Serial.available()) {
    msg = Serial.readString();
    msg.trim();
  }
  return msg;
}

void setup() {
  Serial.begin(115200);
  
  Serial.println("Sensor pronto. Comandos disponiveis:");
  Serial.println("intensidade, frequencia, long, nomes");
}

void loop() {
  String comandoIn = getMsg();

  // Verifica condição de saída de qualquer modo
  if (comandoIn == "e" || comandoIn == "exit") {
    modoAtual = "";
    Serial.println("\n--- Modo encerrado. Aguardando novo comando ---");
    return;
  }

  // Recebe sub-comandos do modo "nomes" sem alterar o modo principal
  if (modoAtual == "nomes" && (comandoIn == "l" || comandoIn == "p")) {
    if (comandoIn == "l") nomes_longReading = !nomes_longReading;
    if (comandoIn == "p") nomes_pt = !nomes_pt;
    comandoIn = ""; // Limpa para não sobrescrever o modoAtual
  }
  // Se chegou um comando válido que não seja de saída, atualiza o modo
  else if (comandoIn != "") {
    modoAtual = comandoIn;
    Serial.print("\n--- Entrando no modo: "); Serial.print(modoAtual); Serial.println(" ---");
  }

  // --- EXECUÇÃO DOS MODOS (Máquina de Estados) ---

  if (modoAtual == "intensidade") {
    Serial.print("Light Intensity: ");
    fixPrint(sensor.darkness());
    Serial.println(sensor.darkness());
  } 
  
  
  else if (modoAtual == "frequencia") {
    int count[8] = {0};
    int readings = 0;
    unsigned long timer = millis() + 5000;
    Serial.println("Lendo frequencia de cores por 5 segundos...");
    
    while (millis() <= timer) {
      sensor.getColor();
      sensor.rgbToHSB(sensor.R_raw, sensor.G_raw, sensor.B_raw);
      byte c = colorDetection(sensor.hue, sensor.saturation, sensor.brightness);
      if (c >= 0 && c < 8) count[c]++;
      readings++;
    }
    
    for (byte i = 0; i < 8; i++) {
      Serial.print("color "); Serial.print(i); Serial.print(": "); Serial.println(count[i]);
    }
    Serial.print("Leituras por segundo: "); Serial.println(readings / 5);
    modoAtual = ""; // Encerra o modo automaticamente
    delay(5000);
  } 
  
  else if (modoAtual == "long") {
    sensor.getColor();
    sensor.rgbToHSB(sensor.R_raw, sensor.G_raw, sensor.B_raw);
    byte color = longBnWColorDetection(sensor.hue, sensor.saturation, sensor.brightness);

    Serial.print("Red: "); fixPrint(sensor.R_raw); Serial.print(sensor.R_raw);
    Serial.print(" |Green: "); fixPrint(sensor.G_raw); Serial.print(sensor.G_raw);
    Serial.print(" |Blue: "); fixPrint(sensor.B_raw); Serial.print(sensor.B_raw);
    Serial.print(" |color: "); Serial.print(color);
    Serial.print(" |Hue: "); fixPrint(sensor.hue); Serial.print(sensor.hue);
    Serial.print(" |Saturation: "); fixPrint(sensor.saturation); Serial.print(sensor.saturation);
    Serial.print(" |Brightness: "); fixPrint(sensor.brightness); Serial.println(sensor.brightness);
  } 
  
  else if (modoAtual == "nomes") {
    sensor.getColor();
    sensor.rgbToHSB(sensor.R_raw, sensor.G_raw, sensor.B_raw);
    
    byte color = colorDetection(sensor.hue, sensor.saturation, sensor.brightness);
    if (nomes_longReading) {
      Serial.print("|");
      color = longBnWColorDetection(sensor.hue, sensor.saturation, sensor.brightness);
    }
    if (nomes_pt) {
      color = color + 9;
    }
    
    String colors[] = {
      "no color", "black", "blue", "green", "yellow", "red", "white", "brown", "silver",
      "sem cor", "preto", "azul", "verde", "amarelo", "vermelho", "branco", "marrom", "prata"
    };
    
    Serial.println(colors[color]);
  } 
  
  else if (modoAtual == "") {
    // Código base rodando quando nenhum modo específico foi chamado
    sensor.getColor();
    sensor.rgbToHSB(sensor.R_raw, sensor.G_raw, sensor.B_raw);
    byte color = colorDetection(sensor.hue, sensor.saturation, sensor.brightness);

    Serial.print("Red: "); fixPrint(sensor.R_raw); Serial.print(sensor.R_raw);
    Serial.print(" |Green: "); fixPrint(sensor.G_raw); Serial.print(sensor.G_raw);
    Serial.print(" |Blue: "); fixPrint(sensor.B_raw); Serial.print(sensor.B_raw);
    Serial.print(" |color: "); Serial.print(color);
    Serial.print(" |Hue: "); fixPrint(sensor.hue); Serial.print(sensor.hue);
    Serial.print(" |Saturation: "); fixPrint(sensor.saturation); Serial.print(sensor.saturation);
    Serial.print(" |Brightness: "); fixPrint(sensor.brightness); Serial.println(sensor.brightness);
    
  }
}