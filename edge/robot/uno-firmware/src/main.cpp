#include <Arduino.h>
#include <Servo.h>

// ===== Pins =====
#define MOTOR_PIN 9
#define PWM1_PIN  5
#define PWM2_PIN  6
#define SHCP_PIN  2
#define EN_PIN    7
#define DATA_PIN  8
#define STCP_PIN  4
#define Trig_PIN  12
#define Echo_PIN  13
#define LEFT_LINE_TRACJING   A0
#define CENTER_LINE_TRACJING A1
#define RIGHT_LINE_TRACJING  A2

// ===== Command codes (match ESP32) =====
const int Forward=92, Backward=163, Turn_Left=149, Turn_Right=106,
          Top_Left=20, Bottom_Left=129, Top_Right=72, Bottom_Right=34,
          Stop=0, Contrarotate=172, Clockwise=83,
          Moedl1=25, Moedl2=26, Moedl3=27, Moedl4=28,
          MotorLeft=230, MotorRight=231;

Servo MOTORservo;

int Left_Tra_Value, Center_Tra_Value, Right_Tra_Value;
int Black_Line = 400;
int leftDistance=0, middleDistance=0, rightDistance=0;
byte RX_package[3] = {0};
uint16_t angle = 90;
byte order = Stop;
char model_var = 0;
int UT_distance = 0;

void Motor(int Dir, int Speed){
  digitalWrite(EN_PIN, LOW);
  analogWrite(PWM1_PIN, Speed);
  analogWrite(PWM2_PIN, Speed);
  digitalWrite(STCP_PIN, LOW);
  shiftOut(DATA_PIN, SHCP_PIN, MSBFIRST, (uint8_t)Dir);
  digitalWrite(STCP_PIN, HIGH);
}
float SR04(int Trig, int Echo){
  digitalWrite(Trig, LOW); delayMicroseconds(2);
  digitalWrite(Trig, HIGH); delayMicroseconds(10);
  digitalWrite(Trig, LOW);
  float distance = pulseIn(Echo, HIGH, 25000UL) / 58.00;
  delay(10);
  return distance > 0 ? distance : 300;
}

void RXpack_func(){
  if(Serial.available()>0){
    delay(1);
    if(Serial.readBytes(RX_package,3)){
      if(RX_package[0]==0xA5 && RX_package[2]==0x5A){
        order = RX_package[1];
        if      (order==Moedl1) model_var=0;
        else if (order==Moedl2) model_var=1;
        else if (order==Moedl3) model_var=2;
        else if (order==Moedl4) model_var=3;
      }
    }
  }
}

void model1_func(byte orders){
  switch(orders){
    case Stop:         Motor(Stop,0); break;
    case Forward:      Motor(Forward,250); break;
    case Backward:     Motor(Backward,250); break;
    case Turn_Left:    Motor(Turn_Left,250); break;
    case Turn_Right:   Motor(Turn_Right,250); break;
    case Top_Left:     Motor(Top_Left,250); break;
    case Top_Right:    Motor(Top_Right,250); break;
    case Bottom_Left:  Motor(Bottom_Left,250); break;
    case Bottom_Right: Motor(Bottom_Right,250); break;
    case Clockwise:    Motor(Clockwise,250); break;
    case Contrarotate: Motor(Contrarotate,250); break;
    case MotorLeft:    angle = constrain(angle+1,1,180); MOTORservo.write(angle); delay(10); break;
    case MotorRight:   angle = constrain(angle-1,1,180); MOTORservo.write(angle); delay(10); break;
    default: order=0; Motor(Stop,0); break;
  }
}

void model2_func(){
  MOTORservo.write(90);
  UT_distance = (int)SR04(Trig_PIN, Echo_PIN);
  Serial.println(UT_distance);
  middleDistance = UT_distance;
  if (middleDistance <= 25){
    Motor(Stop,0);
    for(int i=0;i<300;i++){ delay(1); RXpack_func(); if(model_var!=1) return; }
    MOTORservo.write(10);
    for(int i=0;i<200;i++){ delay(1); RXpack_func(); if(model_var!=1) return; }
    rightDistance = (int)SR04(Trig_PIN, Echo_PIN);
    Serial.print("rightDistance: "); Serial.println(rightDistance);
    MOTORservo.write(170);
    for(int i=0;i<200;i++){ delay(1); RXpack_func(); if(model_var!=1) return; }
    leftDistance = (int)SR04(Trig_PIN, Echo_PIN);
    Serial.print("leftDistance: "); Serial.println(leftDistance);
    MOTORservo.write(90);

    if ((rightDistance<20)&&(leftDistance<20)){
      Motor(Backward,180); for(int i=0;i<600;i++){ delay(1); RXpack_func(); if(model_var!=1) return; }
      Motor(Contrarotate,250); for(int i=0;i<400;i++){ delay(1); RXpack_func(); if(model_var!=1) return; }
    } else if (rightDistance<leftDistance){
      Motor(Backward,180); for(int i=0;i<400;i++){ delay(1); RXpack_func(); if(model_var!=1) return; }
      Motor(Contrarotate,250); for(int i=0;i<400;i++){ delay(1); RXpack_func(); if(model_var!=1) return; }
    } else {
      Motor(Backward,180); for(int i=0;i<400;i++){ delay(1); RXpack_func(); if(model_var!=1) return; }
      Motor(Clockwise,250); for(int i=0;i<400;i++){ delay(1); RXpack_func(); if(model_var!=1) return; }
    }
  } else {
    Motor(Forward,250);
  }
}

void model3_func(){
  MOTORservo.write(90);
  UT_distance=(int)SR04(Trig_PIN, Echo_PIN);
  Serial.println(UT_distance);
  if (UT_distance<15) Motor(Backward,200);
  else if (UT_distance<=20) Motor(Stop,0);
  else if (UT_distance<=25) Motor(Forward,180);
  else if (UT_distance<=50) Motor(Forward,220);
  else Motor(Stop,0);
}

void model4_func(){
  Left_Tra_Value   = analogRead(LEFT_LINE_TRACJING);
  Center_Tra_Value = analogRead(CENTER_LINE_TRACJING);
  Right_Tra_Value  = analogRead(RIGHT_LINE_TRACJING);

  if (Left_Tra_Value < Black_Line && Center_Tra_Value >= Black_Line && Right_Tra_Value < Black_Line){
    Motor(Forward, 250);
  } else if (Left_Tra_Value >= Black_Line && Center_Tra_Value >= Black_Line && Right_Tra_Value < Black_Line){
    Motor(Contrarotate, 220);
  } else if (Left_Tra_Value >= Black_Line && Center_Tra_Value < Black_Line && Right_Tra_Value < Black_Line){
    Motor(Contrarotate, 250);
  } else if (Left_Tra_Value < Black_Line && Center_Tra_Value < Black_Line && Right_Tra_Value >= Black_Line){
    Motor(Clockwise, 250);
  } else if (Left_Tra_Value < Black_Line && Center_Tra_Value >= Black_Line && Right_Tra_Value >= Black_Line){
    Motor(Clockwise, 220);
  } else if (Left_Tra_Value >= Black_Line && Center_Tra_Value >= Black_Line && Right_Tra_Value >= Black_Line){
    Motor(Stop, 0);
  }
}

void setup(){
  Serial.begin(115200);
  MOTORservo.attach(MOTOR_PIN);
  pinMode(SHCP_PIN, OUTPUT); pinMode(EN_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT); pinMode(STCP_PIN, OUTPUT);
  pinMode(PWM1_PIN, OUTPUT); pinMode(PWM2_PIN, OUTPUT);
  pinMode(Trig_PIN, OUTPUT); pinMode(Echo_PIN, INPUT);
  pinMode(LEFT_LINE_TRACJING, INPUT);
  pinMode(CENTER_LINE_TRACJING, INPUT);
  pinMode(RIGHT_LINE_TRACJING, INPUT);
  MOTORservo.write(angle);
  Motor(Stop,0);
  Serial.println("UNO boot OK");
}

void loop(){
  RXpack_func();
  switch (model_var){
    default:
    case 0: model1_func(order); break;
    case 1: model2_func(); break;
    case 2: model3_func(); break;
    case 3: model4_func(); break;
  }
}
