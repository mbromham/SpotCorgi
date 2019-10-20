#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <IRremote.h>

const int IR_RECV_PIN = A3;
IRrecv irrecv(IR_RECV_PIN);
decode_results results;
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

#define MIN_PULSE_WIDTH 650
#define MAX_PULSE_WIDTH 2000
#define FREQUENCY 50
uint8_t servonum = 0;

int numServos = 8;
int finish[8];
int done[8] = {0, 0, 0, 0, 0, 0, 0, 0};
float base_rate = 500;

String moves[5] = {"srv_start[servonum]", "crouch_pos[servonum]", "sit_pos[servonum]", "stand_pos[servonum]", "Shake"};
//                     0    1   2    3   4    5    6   7
int srv_pos[8]   =  {144, 14,  44, 144, 31, 151, 151, 31};
int stepTable[][8]  = {
  {145,  45,  45, 145, 30, 150, 165, 10},  //Start    OK
  {180,  10,  10, 180,  0, 170, 170,  0},  //Crouch   Left Arrow
  { 35, 155,  35, 155, 50, 130, 170,  0},  //Sit      Right Arrow
  {105,  80,  95, 105, 20, 150, 150, 10},   //Stand    *
  { 35, 155,  35, 155, 160, 130, 170,  0}  //Shake    #
};
int walkTable[][8]  = {
//   0,   1,   2,   3,  4,   5,   6,  7 
  {145,  80,  95, 105, 80, 150, 150, 10},  //Start 0
  {85,   80,  95, 105, 75, 150, 150, 10},  //Start 1
  {87,  80,  95, 105, 70, 150, 150, 10},  //Start 2
  {89,  80,  95, 105, 65, 150, 150, 10},  //Start 3
  {91,  80,  95, 105, 60, 150, 150, 10},  //Start 4
  {93,  80,  95, 105, 55, 150, 150, 10},  //Start 5
  {95,  80,  95, 105, 50, 150, 150, 10},  //Start 6
  {97,  80,  95, 105, 45, 150, 150, 10},  //Start 7
  {99,  80,  95, 105, 40, 150, 150, 10},  //Start 8
  {101,  80,  95, 105, 35, 150, 150, 10},  //Start 9
  {103,  80,  95, 105, 30, 150, 150, 10},  //Start 10
  {105,  80,  95, 105, 20, 150, 150, 10}  //Start 11
};
int irDelay = 150;
//************************************************************************
void setup()
{
  Serial.begin(9600);
  while (! Serial);           // Wait until Serial is ready - Leonardo
  Serial.println("PCA9685 Servo test!");
  Serial.println("Setup IR");
  pwm.begin();
  pwm.setPWMFreq(FREQUENCY);
  irrecv.enableIRIn();        // Start the receiver
  Serial.println("Setup Servos start        ");
  doMove(0);
  delay(1000);
}
// ***********************************************************************
void loop()
{
  readIR();
  delay(irDelay);
}
//
//  FUNCTIONS
//
// allDone ************************************************************************
int allDone()
{
  int c = 0;
  for (int d = 0; d < numServos; d++)   {
    if (done[d] > 0)
    {
      c++;
      /*      Serial.print("d = ");
            Serial.print(d);
            Serial.print("  c = ");
            Serial.println(c);
      */
    }
  }
  if (c == numServos)
  {
    for (int d = 0; d < numServos; d++)
    {
      done[d] = 0;
    }
  }
  return c;
}
// Paced Servo Move ***************************************************************
int PSM(int, int )  // Servonum, Start, finish[servonum], Rate
{
  //  finish[servonum] =  stepTable[moveToDo][i];
  //if (srv_pos[servonum] > finish[servonum]) {
  if (srv_pos[servonum] > finish[servonum]) {
    //    while (srv_pos[servonum] > finish[servonum]) {
    pwm.setPWM(servonum, 0, pulseWidth(srv_pos[servonum]));
    //      servo[servonum].write(srv_pos[servonum]);
    srv_pos[servonum]--;
    //   Serial.print("servo ");
    //   Serial.print(servonum);
    //   Serial.print("angle ");
    //   Serial.println(srv_pos[servonum]);
    delayMicroseconds(base_rate);
    //  }
  }
  else
  {
    //while (srv_pos[servonum] < finish[servonum]) {
    pwm.setPWM(servonum, 0, pulseWidth(srv_pos[servonum]));
    //      servo[servonum].write(srv_pos[servonum]);
    srv_pos[servonum]++;
    //   Serial.print("servo ");
    //   Serial.print(servonum);
    //   Serial.print("angle ");
    //   Serial.println(srv_pos[servonum]);
    delayMicroseconds(base_rate);
    //  }
  }
}

// Pulse Width  *********************************************************
int pulseWidth(int angle)
{
  int pulse_wide, analog_value;
  pulse_wide = map(angle, 0, 180, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH);
  analog_value = int(float(pulse_wide) / 1000000 * FREQUENCY * 4096);
  return analog_value;
}

// doMove *****************************************************************
int doMove(int moveToDo) {
  Serial.print("Setting servos to ");
  Serial.print(moveToDo);
  Serial.print(" which is ");
  Serial.println(moves[moveToDo]);
  //delay(250);
  do  {
    for (int i  = 0; i < numServos; i++) {
      servonum = i;
      finish[servonum] = stepTable[moveToDo][i];
      if (srv_pos[servonum] != finish[servonum])
      {
        PSM(servonum, stepTable[moveToDo][i]);
      }
      else
      {
        done[servonum] = 1;
      }
      delayMicroseconds(base_rate);
      // Serial.print("Servonum ");
      //  Serial.print(servonum);
      //   Serial.print(" srv_start = ");
      //    Serial.println(stepTable[moveToDo][i]);
    }
  }
  while (allDone() < numServos);
}
// READIR *****************************************************************
int readIR() {
  if (irrecv.decode(&results)) {
    Serial.println(results.value, HEX);
    //irrecv.enableIRIn(); // Start the receiver
    //delay(150);
    //    irrecv.resume(); // Receive the next value
    //  }
    //  if (irrecv.decode(&results)) {
    switch (results.value) {
      case 0xFF9867: // 0
        Serial.println("Servonum 0 ");
        servonum = 0;
        break ;
      case 0xFFA25D:
        Serial.println("Servonum 1");
        servonum = 1;
        break;
      case 0xFF629D:
        Serial.println("Servonum 2 ");
        servonum = 2;
        break;
      case 0xFFE21D:
        Serial.println("Servonum 3 ");
        servonum = 3;
        break;
      case 0xFF22DD:
        Serial.println("Servonum 4 ");
        servonum = 4;
        break;
      case 0xFF02FD:
        Serial.println("Servonum 5 ");
        servonum = 5;
        break ;
      case 0xFFC23D:
        Serial.println("Servonum 6 ");
        servonum = 6;
        break ;
      case 0xFFE01F:
        Serial.println("Servonum 7 ");
        servonum = 7;
        break ;
      case 0xFFA857:
        Serial.println("Servonum 8 ");
        break ;
      case 0xFF906F:
        Serial.println("Servonum 9 ");
        break ;
      case 0xFF6897: // *
        Serial.print("  *  Asterisk");
        doMove(3);
        break ;
      case 0xFFB04F: // #
        Serial.print("  # Hash");
        doMove(4);
        delay(500);
        pwm.setPWM(0, 0, pulseWidth(0));
        delay(200);
        pwm.setPWM(0, 0, pulseWidth(30));
        delay(600);
        pwm.setPWM(0, 0, pulseWidth(0));
        delay(200);
        pwm.setPWM(0, 0, pulseWidth(30));
        delay(1000);
        doMove(2);        
        break ;
      case 0xFF38C7: // OK
        Serial.println("Setup Servos");
        //        setupServos();
        doMove(0);
        break ;
      case 0xFF4AB5:               //Down Arrow
        Serial.println("Down Arrow. ");
        Serial.print("Servo ");
        Serial.print(servonum);
        Serial.print(" = ");
        Serial.print(finish[servonum]);
        finish[servonum] = srv_pos[servonum] - 10;
        pwm.setPWM(servonum, 0, pulseWidth(finish[servonum]));
        //        servo[servonum].write(finish[servonum]);
        delay(100);
        srv_pos[servonum] = finish[servonum];
        Serial.print(" srv_pos = ");
        Serial.println(srv_pos[servonum]);
        break ;
      case 0xFF18E7:                //Up Arrow
        Serial.println("Up Arrow. ");
        Serial.print("Servo ");
        Serial.print(servonum);
        Serial.print(" = ");
        Serial.print(finish[servonum]);
        finish[servonum] = srv_pos[servonum] + 10;
        pwm.setPWM(servonum, 0, pulseWidth(finish[servonum]));
        //        servo[servonum].write(finish[servonum]);
        delay(100);
        srv_pos[servonum] = finish[servonum];
        Serial.print(" srv_pos = ");
        Serial.println(srv_pos[servonum]);
        break ;
      case 0xFF10EF:                     // Left Arrow
        Serial.print("Left Arrow  ");
        doMove(1);
        //delay(1000);
        //        walkStart();
        break ;
      case 0xFF5AA5:                     // Right Arrow
        Serial.print("Right Arrow  ");
        doMove(2);
        //  walkTest(servonum);
        break;
    }
    Serial.print("IR Read as ");
    Serial.print(results.value, HEX);
    Serial.print(" which is servonum ");
    Serial.println(servonum);
  }
  delay(irDelay);
  irrecv.resume(); // Receive the next value
}
// Display Results **************************************************************
int displayResults() {
  for (int i = 0; i < numServos; i++) {
    Serial.print("DR Servo ");
    Serial.print(i);
    Serial.print(" srv_pos ");
    //    Serial.println(srv_pos[i]);
  }
}
