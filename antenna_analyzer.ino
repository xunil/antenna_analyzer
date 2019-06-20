#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <Fonts/FreeMono9pt7b.h>
#include <SPI.h>
#include "si5351.h"
#include <Wire.h>

Si5351 si5351;

#define TFT_CS        10
#define TFT_RST        9 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC         8

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

#define GRAPH_LEFT    24
#define GRAPH_TOP      8
#define GRAPH_RIGHT  120
#define GRAPH_BOTTOM 104

#define GRAPH_HEIGHT (GRAPH_BOTTOM-GRAPH_TOP)
#define GRAPH_WIDTH  (GRAPH_RIGHT-GRAPH_LEFT)
#define PIP_WIDTH 3
#define PIP_INCREMENT 32
#define PIP_COUNT 3

#define FWD_PIN A0
#define REV_PIN A1
#define NUM_READINGS 10

#define ADC_RESOLUTION 12
#define ADC_DIVISOR (2**ADC_RESOLUTION)


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

#define NUM_DATA_POINTS 96
float swrReadings[96];

float maxSWR = 10.0;

void setup() {
  SerialUSB.begin(115200);
  analogReference(AR_EXTERNAL);
  analogReadResolution(12);
  pinMode(FWD_PIN, INPUT);
  pinMode(REV_PIN, INPUT);
  tft.initR(INITR_144GREENTAB); // Init ST7735R chip, green tab
  initFreqSource();
  drawGraphAxes();
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
  unsigned long fwd, rev;
  for(i = 0, l = start_f; l <= end_f; i++, l += step) {
    // Set Si5351 to tested frequency
    si5351.set_freq(l * 100ULL, SI5351_CLK0);
    fwd = 0; rev = 0;
    // Take NUM_READINGS values from the forward and reverse ADCs
    for (int j = 0; j < NUM_READINGS; j++) {
      fwd += analogRead(FWD_PIN);
      rev += analogRead(REV_PIN);
    }
    // Average the forward and reverse readings at the tested frequency 
    fwd /= NUM_READINGS;
    rev /= NUM_READINGS;

    float ffwd, frev;
    ffwd = (float)fwd;
    frev = (float)rev;

    float rho = sqrt(frev/ffwd);
    float swr = (1.0+rho)/(1.0-rho);

    swrReadings[i] = swr;
    SerialUSB.print("Freq: ");
    SerialUSB.print((float)(l/1000000.0), 3);
    SerialUSB.print("MHz fwd: ");
    SerialUSB.print(ffwd);
    SerialUSB.print(" rev: ");
    SerialUSB.print(frev);
    SerialUSB.print(" rho: ");
    SerialUSB.print(rho, 4);
    SerialUSB.print(" swr: ");
    SerialUSB.println(swr, 4);
  }
}

void drawGraphAxes() {
  // Clear the screen
  tft.fillScreen(ST77XX_BLACK);

  // Draw SWR graph border
  tft.drawFastVLine(GRAPH_LEFT-1, GRAPH_TOP, GRAPH_HEIGHT+1, ST77XX_YELLOW);
  tft.drawFastVLine(GRAPH_RIGHT+1, GRAPH_TOP, GRAPH_HEIGHT+1, ST77XX_YELLOW);
  tft.drawFastHLine(GRAPH_LEFT-1, GRAPH_BOTTOM+1, GRAPH_WIDTH+3, ST77XX_YELLOW);
}

unsigned int swr_to_graph_y(float swrReading) {
  return (unsigned int)(GRAPH_HEIGHT - (GRAPH_HEIGHT * (swrReading / maxSWR)));
}

void drawGraph(float *graph, int numpoints) {
  for (int i=0; i<numpoints; i++) {
#if 0    
    tft.drawPixel(i+GRAPH_LEFT+1, (GRAPH_HEIGHT-graph[i])+GRAPH_TOP-1, ST77XX_RED);
#else
    tft.drawPixel(i+GRAPH_LEFT+1, swr_to_graph_y(graph[i]), ST77XX_GREEN);
#endif
  }
}

void clearGraph() {
  tft.fillRect(GRAPH_LEFT, GRAPH_TOP, GRAPH_WIDTH, GRAPH_HEIGHT, ST77XX_BLACK);
  drawLegend();
}

void drawPips() {
  tft.setFont();
  tft.setTextSize(1);
  for (int i=0; i<PIP_COUNT; i++) {
    int pip_y = GRAPH_TOP+((GRAPH_HEIGHT/PIP_COUNT)*i);
    tft.drawFastHLine(GRAPH_LEFT, pip_y, PIP_WIDTH, ST77XX_WHITE);
    tft.drawFastHLine((GRAPH_RIGHT-PIP_WIDTH)+1, pip_y, PIP_WIDTH, ST77XX_WHITE);

    tft.setCursor(1, pip_y+4);
    tft.setTextColor(pipColors[i]);
    tft.print(maxSWR-((maxSWR/PIP_COUNT)*i), 1);
  }
}

void drawLegend() {
  tft.setFont();
  tft.setTextSize(1);
#if 0
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
#endif

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(0, 118);
  tft.print("CTR:14.175");  // FIXME: actually reference the band we're using
  tft.setCursor(68, 118); // FIXME: calculate string width to determine X
  tft.print("SPN:350kHz");// FIXME: use actual calculated frequency span
}

void loop() {
  // Read the Status Register and print it every 10 seconds
  si5351.update_status();
  SerialUSB.print("SYS_INIT: ");
  SerialUSB.print(si5351.dev_status.SYS_INIT);
  SerialUSB.print("  LOL_A: ");
  SerialUSB.print(si5351.dev_status.LOL_A);
  SerialUSB.print("  LOL_B: ");
  SerialUSB.print(si5351.dev_status.LOL_B);
  SerialUSB.print("  LOS: ");
  SerialUSB.print(si5351.dev_status.LOS);
  SerialUSB.print("  REVID: ");
  SerialUSB.println(si5351.dev_status.REVID);

#if 0
  delay(10000);
  if (si5351.dev_status.SYS_INIT == 0 && si5351.dev_status.LOL_A == 0 && si5351.dev_status.LOL_B == 0 && si5351.dev_status.LOS == 0) {
    clearGraph();
    sweep(14000000ULL, 14350000ULL, graphDataPoints);
    drawGraph(graphData, graphDataPoints);
  }
  drawLegend();
#endif

  SerialUSB.print("sweep");

  sweep(14000000ULL, 14350000ULL, NUM_DATA_POINTS);
  SerialUSB.print("clearGraph");
  clearGraph();
  SerialUSB.print("drawGraph");  
  drawGraph(swrReadings, NUM_DATA_POINTS);
  SerialUSB.print("drawPips");
  drawPips();
  SerialUSB.print("drawLegend");
  drawLegend();
  SerialUSB.print("delay");
  delay(1000);
}
