/**
 * 電流電圧計 For ATTiny85 / ArduinoUNO
 * ※ピン番号を変更すればArduinoIDE対応のマイコンなら大体動きます
 */
#include <Wire.h>
#include "DISP7SEG.h"
DISP7SEG disp ;

#define INA219_ADDR 0x40
// --- ATTiny85
#define MODE_PIN 1  // 電流/電圧　測定モード切替
#define SW_PIN 3    // 計測レンジ切り替えスイッチ
#define LED_PIN 4   // 計測レンジ表示ＬＥＤ

// --- ArduinoUNO
//#define MODE_PIN 8
//#define SW_PIN 7
//#define LED_PIN 13
// -------------- 

int measuMode = 0 ; // 0:電流計 1:電圧計
int rangeMode = 0 ; // 0:32V/320mA 1:16V/400mA
double preDispVal = 0.0 ; // 書き換え抑止用
int preRangeMode = 0 ;    // 書き換え抑止用
int preMeasuMode = 0 ;    // 書き換え抑止用

static uint8_t conf[] = {
  0x399F , // 32V / 320mA / 12Bit ※ディフォルト
  0x019F , // 16V / 40mA / 12Bit
 } ;

const double rangeV = 0.001 ; // 電圧分解能
const double rangeA = 0.1 ;   // 電流分解能

// ----------------------
// ----- 静電容量表示 -----
// ----------------------
void dispConv(double dispVal,int mode) {
                    // 01234567 01234567
  int  dispSeg[9] ; // xxx.xxxV ±xxx.xmA

  if (mode == 1) {  // 0:電流計 1:電圧計
    // --- 電圧表示
    for (int i=0;i<8;i++) {
      dispSeg[i] = 416 ;  // 空白で埋める
    }
    dispSeg[7] = 24 ; // V

    int range1 = dispVal ;
    int range2 = dispVal * 1000.0 ;

    int pos1 = 2 ;
    int pos2 = 4 ;
    int pos2l = 3 ;
    dispSeg[3] = 20 ; // DOT
    for (int i=pos1;i>=0;i--) {
      dispSeg[i] = range1 % 10 ;
      range1 /= 10 ;
      if (range1 == 0) break ;
    }
    for (int i=pos2l-1;i>=0;i--) {
      dispSeg[pos2+i] = range2 % 10 ;
      range2 /= 10 ;
    }
  } else
  if (mode == 0) {  // 0:電流計 1:電圧計
    // --- 電流表示
    for (int i=0;i<8;i++) {
      dispSeg[i] = 416 ;  // 空白で埋める
    }
    dispSeg[6] = 25 ; // m
    dispSeg[7] = 23 ; // A

    int range1 = dispVal ;
    int range2 = dispVal * 10.0 ;

    int pos1 = 3 ;
    int pos1e = 0 ;
    int pos2 = 5 ;
    int pos2l =1 ;

    if (dispVal < 0) {
      range1 = -1 * range1 ;
      range2 = -1 * range2 ;
    } else {
      pos1e = 1 ;
      dispSeg[0] = 21 ; // -
    }

    dispSeg[4] = 20 ; // DOT
    for (int i=pos1;i>=pos1e;i--) {
      dispSeg[i] = range1 % 10 ;
      range1 /= 10 ;
      if (range1 == 0) break ;
    }

    for (int i=pos2l-1;i>=0;i--) {
      dispSeg[pos2+i] = range2 % 10 ;
      range2 /= 10 ;
    }
  } else {
    // --- 初期画面 8888.888 表示
    for (int i=0;i<8;i++) {
      dispSeg[i] = 22 ;
    }
    dispSeg[4] = 20 ; // DOT
    disp.cls() ;
  }

  // --- 表示
  int x = 0 ;
  for (int i=0;i<8;i++) {
    disp.disp7SEG(x,0,dispSeg[i]) ;
    x += (dispSeg[i] == 20) ? 8 : 16 ; // DOTだけ幅を狭める
  } 
}

void setup() {
  pinMode(MODE_PIN,INPUT) ;
  pinMode(SW_PIN,INPUT) ;
  pinMode(LED_PIN,OUTPUT) ;
  digitalWrite(LED_PIN,LOW) ;
 
  Wire.begin( ) ;
  disp.init() ;

  measuMode = -1 ;
  dispConv(0.0,measuMode) ;
}

void loop() {
  double dispVal = 0.0 ;
  int measuModeCH = (digitalRead(MODE_PIN) == HIGH) ? 0 : 1 ;   // 0:電流計 1:電圧計
  int rangeModeCH = rangeMode ;

  if (digitalRead(SW_PIN) == LOW) {
    // - 測定レンジ切り替え
    rangeModeCH = (rangeMode + 1) % 2 ;
    digitalWrite(LED_PIN,((rangeModeCH==0)?LOW:HIGH)) ;
    delay(1000) ;
  }

  if (measuModeCH != measuMode || rangeModeCH != rangeMode) {
    // - 計測モード、測定レンジが切り替わった場合にCONFIG値を再設定する
    measuMode = measuModeCH ;
    rangeMode = rangeModeCH ;
    Wire.beginTransmission(INA219_ADDR);
    Wire.write(0x00);                     // config アドレス
    Wire.write((conf[rangeMode] >> 8) & 0xFF);
    Wire.write(conf[rangeMode] & 0xFF);
    Wire.endTransmission();  
  }

  if (measuMode == 1) { // 0:電流計 1:電圧計
    // ----- 電圧 -----
    Wire.beginTransmission(INA219_ADDR);
    Wire.write(0x02);                    //BusVoltageレジスタ
    Wire.endTransmission();
    Wire.requestFrom(INA219_ADDR, 2);
    while (Wire.available() < 2);
    int16_t voltI = Wire.read() << 8 | Wire.read();
    voltI = (voltI >> 3) * 4;
    dispVal = voltI * rangeV ;
  } else {  // 0:電流計 1:電圧計
    // ----- 電流 -----
    Wire.beginTransmission(INA219_ADDR);
    Wire.write(0x01);
    Wire.endTransmission();
    Wire.requestFrom(INA219_ADDR, 16);
    while (Wire.available() < 2);
    int16_t shuntVoltI = Wire.read() << 8 | Wire.read();
    dispVal = shuntVoltI * rangeA ;
  }

  // チラツキ防止：表示に変更がない場合は表示処理を行わない
  if (preDispVal != dispVal || preMeasuMode != measuMode || preRangeMode != rangeMode) {
    preDispVal = dispVal ;
    preMeasuMode = measuMode ;
    preRangeMode = rangeMode ;
    
    dispConv(dispVal,measuMode) ;
  }

  delay(200) ;
}
