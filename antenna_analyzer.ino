#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <Fonts/FreeMono9pt7b.h>
#include <SPI.h>
#include "si5351.h"
#include <Wire.h>

Si5351 si5351;

#define TFT_CS        41
#define TFT_RST       43 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC        45

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

#define GRAPH_LEFT    24
#define GRAPH_TOP      8
#define GRAPH_RIGHT  120
#define GRAPH_BOTTOM 104

#define GRAPH_HEIGHT (GRAPH_BOTTOM-GRAPH_TOP)
#define GRAPH_WIDTH  (GRAPH_RIGHT-GRAPH_LEFT)
#define PIP_WIDTH 3
#define PIP_INCREMENT 32

#define FWD_PIN A0
#define REV_PIN A1
#define NUM_READINGS 5


const char *pipLabels[] = {"2.5", "2.0", "1.5", "1.0"};
const int pipColors[] = {ST77XX_RED, ST77XX_ORANGE, ST77XX_YELLOW, ST77XX_WHITE};

const long fakeGraphData[] = {
  1022, 988, 955, 921, 887, 854, 821, 787, 755, 722, 690, 658, 627, 596, 565,
  535, 506, 477, 449, 421, 394, 368, 342, 317, 293, 270, 247, 226, 205, 185,
  166, 148, 131, 115, 100, 86, 72, 60, 49, 40, 31, 23, 16, 11, 6, 3, 1, 0, 0, 1,
  3, 6, 11, 16, 23, 31, 40, 50, 61, 73, 86, 100, 115, 131, 148, 166, 185, 205,
  226, 248, 270, 293, 318, 342, 368, 394, 421, 449, 477, 506, 536, 566, 596,
  627, 659, 691, 723, 755, 788, 821, 854, 888, 921, 955, 989, 1023
};

const int fakeGraphPoints = 96;

long graphData[96];
int graphDataPoints = 96;

void setup() {
  Serial.begin(115200);
  //analogReference(INTERNAL1V1);
  pinMode(FWD_PIN, INPUT);
  pinMode(REV_PIN, INPUT);
  tft.initR(INITR_144GREENTAB); // Init ST7735R chip, green tab
  initFreqSource();
  drawGraphAxes();
}

void drawDongs() {
  tft.setCursor(48, 48);
  tft.setFont();
  tft.setTextSize(2);
  tft.print("8==D");
}

void initFreqSource() {
  bool i2c_found;

  i2c_found = si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  if(!i2c_found) {
    Serial.println("Device not found on I2C bus!");
  }

  // Set CLK0 to output 14.175 MHz
  si5351.set_freq(1417500000ULL, SI5351_CLK0);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
  si5351.output_enable(SI5351_CLK0, 1);

  // Query a status update and wait a bit to let the Si5351 populate the
  // status flags correctly.
  si5351.update_status();
  delay(1000);
}

void sweep(unsigned long long start_f, unsigned long long end_f, unsigned int points) {
  unsigned long long step = (end_f - start_f) / points;
  unsigned long long l;
  int i;
  long fwd, rev;
  for(i = 0, l = start_f; l <= end_f; i++, l += step) {
    si5351.set_freq(l * 100ULL, SI5351_CLK0);
    fwd = 0; rev = 0;
    for (int j = 0; j < NUM_READINGS; j++) {
      fwd += analogRead(FWD_PIN);
      rev += analogRead(REV_PIN);
    }
    fwd /= NUM_READINGS;
    rev /= NUM_READINGS;
    float ffwd, frev;
    ffwd = fwd / 1024.0;
    frev = rev / 1024.0;
    float swr = (fwd+rev)/(fwd-rev);
    //float swr = (ffwd+frev)/(ffwd-frev);

    graphData[i] = GRAPH_HEIGHT * swr;
    Serial.print("Freq: ");
    Serial.print((float)(l/1000000.0));
    Serial.print(" fwd: ");
    Serial.print(ffwd);
    Serial.print(" rev: ");
    Serial.print(frev);
    Serial.print(" swr: ");
    Serial.println(swr);
  }
}

void drawGraphAxes() {
  // Clear the screen
  tft.fillScreen(ST77XX_BLACK);

  // Draw SWR graph border
  tft.drawFastVLine(GRAPH_LEFT-1, GRAPH_TOP, GRAPH_HEIGHT+1, ST77XX_YELLOW);
  tft.drawFastVLine(GRAPH_RIGHT+1, GRAPH_TOP, GRAPH_HEIGHT+1, ST77XX_YELLOW);
  tft.drawFastHLine(GRAPH_LEFT-1, GRAPH_BOTTOM+1, GRAPH_WIDTH+2, ST77XX_YELLOW);
}

void drawGraph(long *graph, int numpoints) {
  for (int i=0; i<numpoints; i++) {
    tft.drawPixel(i+GRAPH_LEFT+1, (GRAPH_HEIGHT-map(graph[i], 0, 1023, 0, 96))+GRAPH_TOP, ST77XX_GREEN);
  }
}

void clearGraph() {
  fillRect(GRAPH_LEFT, GRAPH_TOP, GRAPH_WIDTH, GRAPH_HEIGHT, ST77XX_BLACK);
  drawLegend();
}

void drawLegend() {
  tft.setFont();
  tft.setTextSize(1);
  for (int i=0; i<(GRAPH_HEIGHT/PIP_INCREMENT); i++) {
    // Draw pips for SWR levels
    tft.drawFastHLine(GRAPH_LEFT, GRAPH_TOP+(PIP_INCREMENT*i), PIP_WIDTH, ST77XX_WHITE);
    tft.drawFastHLine((GRAPH_RIGHT-PIP_WIDTH)+1, GRAPH_TOP+(PIP_INCREMENT*i), PIP_WIDTH, ST77XX_WHITE);
    // Label the pips 
    tft.setCursor(1, (i*PIP_INCREMENT)+4);
    tft.setTextColor(pipColors[i]);
    tft.print(pipLabels[i]);
  }

  int idx = (GRAPH_HEIGHT/PIP_INCREMENT);
  tft.setCursor(1, (idx*PIP_INCREMENT)+4);
  tft.setTextColor(ST77XX_GREEN);
  tft.print(pipLabels[idx]);

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(0, 118);
  tft.print("CTR:14.175");  // FIXME: actually reference the band we're using
  tft.setCursor(68, 118); // FIXME: calculate string width to determine X
  tft.print("SPN:350kHz");// FIXME: use actual calculated frequency span
}

void loop() {
  // Read the Status Register and print it every 10 seconds
  si5351.update_status();
  Serial.print("SYS_INIT: ");
  Serial.print(si5351.dev_status.SYS_INIT);
  Serial.print("  LOL_A: ");
  Serial.print(si5351.dev_status.LOL_A);
  Serial.print("  LOL_B: ");
  Serial.print(si5351.dev_status.LOL_B);
  Serial.print("  LOS: ");
  Serial.print(si5351.dev_status.LOS);
  Serial.print("  REVID: ");
  Serial.println(si5351.dev_status.REVID);

  delay(10000);
  if (si5351.dev_status.SYS_INIT == 0 && si5351.dev_status.LOL_A == 0 && si5351.dev_status.LOL_B == 0 && si5351.dev_status.LOS == 0) {
    clearGraph();
    sweep(14000000ULL, 14350000ULL, graphDataPoints);
    drawGraph(graphData, graphDataPoints);
  }
  drawLegend();
}
