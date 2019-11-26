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

int numServos = 8;                          // Number of servos
int finish[8];                              // Servo move target placeholder
int done[8] = {0, 0, 0, 0, 0, 0, 0, 0};     // Flag for move finished
int baseRate = 1;                           // Determine pace of servo moves to set positions
int walkRate = 20;                          // Determine pace of servo moves during Walk
int stepCount = 3;                          // How many steps to take in Walk or moves in singleMove
int irDelay = 150;                          // Delay for IR reads
int lowerStart = 100;                       // Servo 0 start
int upperStart = 50;                        // Servo 4 start
int liftSize = 15;                          // Lower leg raise distance
int stepSize = 20;                          // Upper leg move distance. Array fill crashes if > 48. Don't know why.
int moveCount = 0;                          // Number of moves in walkTable. Defined during array fill.
int moveNum = 0;                            // Move counter for singleMove
int usCountdown;                            // Upper start countdown holder for moves to return to start
float legArc[100];                          // Holder for circular function to adjust lower leg angle to keep body height constant as upper leg moves back (currently disabled for testing)
int legStartDelay;                          // Moves before a specific leg steps. Defined as fraction of moveCount, usually 1/4.
int walkTable[200][8] = {lowerStart, 0, 0, 0, upperStart, 0, 0, 0};  // Array of positions for L & U (0 & 4). Not all 100 are used but need to be specified to reserve memory.

String moves[9] = {"Start", "Crouch", "Sit", "Stand", "Tweak", "Shake", "Reset", "Single Step"};
int legAdjust[8] = {0, 0, 0, 0, 0, 0, 0, 0};            // Adjust walk angle for each leg
int moveTable[5][8]  = {                                  //Servo settings for preset positions
  // 0,   1,  2,   3,  4,   5,  6,  7
  {109,  71, 81,  99, 61, 119, 119, 59},   //Start
  {180,   0,  0, 180,  0, 180, 180, 0},    //Crouch   Down Arrow
  { 40, 140, 10, 170, 90,  90, 150, 30},   //Sit      Left Arrow
  {110,  70, 90,  90, 60, 120, 120, 60},   //Stand    Right Arrow
  {110,  70, 90,  90, 60, 120, 120, 60}    //Placeholder for current servo position
};
int paths[4][8] {
  {0, 0, 0, 0, 0, 0, 0, 0},            //Forward
  { -1, -1, -1, -1, -1, -1, -1, -1},   //Reverse
  { -1, 0, 0, -1, -1, 0, 0, -1},       //Right
  {0, -1, -1, 0, 0, -1, -1, 0}         // Left
};

//************************************************************************
void setup()
{
  Serial.begin(9600);
  while (! Serial);                       // Wait until Serial is ready
  Serial.println("SpotCorgi 6.2 test");
  Serial.println("Setup IR");
  pwm.begin();
  pwm.setPWMFreq(FREQUENCY);
  irrecv.enableIRIn();                    // Start the receiver
  fillwalkTable();                        // Create move positions array
  Serial.println("Setup finished");
  delay(1000);
}
//************************************************************************
void loop()
{
  readIR();                               // Read new instruction from IR receiver
  delay(irDelay);
}
//
// FUNCTIONS  ************************************************************
//
int Walk(int path) {                     // Take one step for all legs

  for (int p  = 0; p < moveCount; p++) {
    for (int i  = 0; i < numServos; i++) {
      servonum = i;
      int q = abs(p - paths[path][i] + (paths[path][i] * moveCount));
      finish[servonum] = walkTable[q][i];
      pwm.setPWM(servonum, 0, pulseWidth(finish[servonum]));
      delay(walkRate);
      moveTable[4][servonum] = finish[servonum];
    }
  }
}
//************************************************************************
int tweakVariables(int serv, int fb) {       // Adjust walk variables using remote
  int tweaks[2][8] =
  { {  1, -1, -1, 1, 1, -1, -1, 1},                  // Move leg walk start Forward
    { -1, 1, 1, -1, -1, 1, 1, -1}                  // Move leg walk start Backward
  };
  for (int s = 0; s < numServos; s++) {
    //moveTable[4][s] = servoPos[s];
  }
  for (int i = 0; i < 2; i++) {              // Set servos
    moveTable[4][i + serv] = moveTable[4][i + serv]  + tweaks[fb][i + serv];
    legAdjust[i + serv] = legAdjust[i + serv] + tweaks[fb][i + serv];
  }
  doMove(4);
  //  displayResults(4);

  Serial.println(" ");
}
// ************************************************************************
int fillwalkTable()                                    // Writes array of positions all servos to Walk
{
  walkTable[0][0] = lowerStart + legAdjust[0];                        // Set start, lift & move step values for servos 0 & 4
  walkTable[0][4] = upperStart + legAdjust[4];
  legArc[0] = 0;
  walkTable[1][0] = lowerStart + liftSize + legAdjust[0];
  walkTable[1][4] = upperStart + stepSize + legAdjust[4];
  legArc[1] = 0;
  walkTable[2][0] = lowerStart + legAdjust[0];
  walkTable[2][4] = upperStart + stepSize + legAdjust[4];
  legArc[2] = 0;
  /*
    walkTable[3][0] = lowerStart + liftSize;
    walkTable[3][4] = upperStart + stepSize;
    legArc[3] = 0;
  */
  usCountdown = walkTable[1][4];
  moveCount  = 3;
  do {                                                 // Complete the rest of the moves for 0 & 4 counting down Upper to start position.
    usCountdown--;
    walkTable[moveCount][0] = lowerStart + legAdjust[0];
    walkTable[moveCount][4] = usCountdown + legAdjust[4];
    moveCount++;
    moveNum++;
  }
  while (usCountdown > upperStart + 1);
  for (int m = 4; m < moveCount; m++) {               // Rebuild array to adjust lower leg angle to keep body height constant as upper leg moves back
    // legArc[m] = sqrt(pow(((moveCount - 4) / 2), 2) - pow((((moveCount - 4) / 2) - m + 4), 2));
    legArc[m] = 0;      // Disable for testing)
  }

  for (int m = 0; m < moveCount; m++) {               // Duplicate 0 & 4 array to allow for delayed start of each leg
    walkTable[moveCount + m][0] = walkTable[m][0];
    walkTable[moveCount + m][4] = walkTable[m][4];
  }

  for (int m = 0; m < moveCount; m++) {               // Write out full Walk array for all servos including staggered starts and angle adjustments
    legStartDelay = moveCount / 4;                    // Moves to wait before starting a specific leg
    walkTable[m][0] = walkTable[m][0] + legArc[m];     // Adjustments and Leg start delays included
    walkTable[m][1] = 180 - (walkTable[m + (legStartDelay)][0]); // + legArc[m]);
    walkTable[m][2] = 180 - (walkTable[m + (legStartDelay * 3)])[0]; // + legArc[m]);
    walkTable[m][3] = walkTable[m + (legStartDelay * 2)][0] + legArc[m];
    walkTable[m][4] = walkTable[m][4];
    walkTable[m][5] = 180 - (walkTable[m + (legStartDelay)][4]);
    walkTable[m][6] = 180 - (walkTable[m + (legStartDelay * 3)][4]);
    walkTable[m][7] = walkTable[m + (legStartDelay * 2)][4];
  }
  walkRate = 40 - moveCount;
  if (walkRate < 10) walkRate = 10;
  Serial.println("Fill walkTable");
  Serial.println("Servo\t\t0\t1\t2\t3\t4\t5\t6\t7");
  for (int c = 0; c < moveCount; c++) {               // Display array values
    Serial.print("Move =\t");
    Serial.print(c);
    Serial.print("\t");
    for (int s = 0; s < numServos; s++)  {
      Serial.print(walkTable[c][s]);
      Serial.print(",\t");
    }
    Serial.print("legArc =");
    Serial.print(legArc[c]);
    Serial.println(" ");
  }
  Serial.println(" ");
  Serial.println("Variables");
  Serial.print("moveCount =\t");
  Serial.println(moveCount);
  Serial.print("walkRate  =\t");
  Serial.println(walkRate);
  Serial.print("stepSize  =\t");
  Serial.println(stepSize);
  Serial.print("liftSize  =\t");
  Serial.println(liftSize);
  Serial.print("Steps     =\t ");
  Serial.println(stepCount);
  delay(500);
  moveNum = 0;
  Serial.println(" ");
  doMove(3);                                         // Move to Stand position
}

//************************************************************************
int PSM(int, int )                        // Paced Servo Move - Move servo from start to finish by 1deg steps
{
  if (moveTable[4][servonum] > finish[servonum]) {
    pwm.setPWM(servonum, 0, pulseWidth(moveTable[4][servonum]));
    moveTable[4][servonum]--;
    delay(baseRate);
  }
  else
  {
    pwm.setPWM(servonum, 0, pulseWidth(moveTable[4][servonum]));
    moveTable[4][servonum]++;
    delay(baseRate);
  }
}
//************************************************************************
int pulseWidth(int angle)                 // Convert angles to PWM frequencies
{
  int pulse_wide, analog_value;
  pulse_wide = map(angle, 0, 180, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH);
  analog_value = int(float(pulse_wide) / 1000000 * FREQUENCY * 4096);
  return analog_value;
}
//************************************************************************
int doMove(int moveToDo) {                // Set servos to preset positions
  do  {
    for (int i  = 0; i < numServos; i++) {
      servonum = i;
      finish[servonum] = moveTable[moveToDo][i];
      if (moveTable[4][servonum] != finish[servonum])
      {
        PSM(servonum, moveTable[moveToDo][i]);
      }
      else
      {
        done[servonum] = 1;
      }
      delay(baseRate);
    }
  }
  while (allDone() < numServos);
  Serial.print("Set Servos to: ");
  Serial.println(moves[moveToDo]);
  displayResults(moveToDo);
}
//************************************************************************
int singleMove()                          // Take stepCount walk moves
{
  for (int passes = 0; passes < stepCount; passes++) {
    Serial.print("Move ");
    Serial.println(moveNum);
    for (int i  = 0; i < numServos; i++) {
      servonum = i;
      finish[servonum] = walkTable[moveNum][i];
      pwm.setPWM(servonum, 0, pulseWidth(finish[servonum]));
      moveTable[4][servonum] = finish[servonum];
    }
    displayResults(7);
    delay(500);
    moveNum++;
    if (moveNum > moveCount - 1) moveNum = 0;
  }
}

//************************************************************************
int readIR() {                              // Read commands from IR
  if (irrecv.decode(&results)) {
    Serial.println(results.value, HEX);
    switch (results.value) {
      case 0x807FE817: // 0
        //Select servo for adjustemnt
        Serial.println("Servonum 0 ");
        servonum = 0;
        break ;
      case 0x807F4AB5:                     // 1 - Crouch  *********************
        Serial.println("Down Arrow - Crouch");
        //doMove(1);
        Serial.println("Servonum 1");
        servonum = 1;
        break;
      case 0x807F0AF5:                     // 2 - Sit  *********************
        Serial.println("Left Arrow - Sit");
        //doMove(2);
        Serial.println("Servonum 2 ");
        servonum = 2;
        break;
      case 0x807F08F7:                     // 3 - Stand  *********************
        Serial.println("Right Arrow - Stand");
        //doMove(3);
        Serial.println("Servonum 3 ");
        servonum = 3;
        break;
      case 0x807F6A95:
        Serial.println("Servonum 4 ");
        servonum = 4;
        break;
      case 0x807F2AD5:
        Serial.println("Servonum 5 ");
        servonum = 5;
        break ;
      case 0x807F28D7:
        Serial.println("Servonum 6 ");
        servonum = 6;
        break ;
      case 0x807F728D:
        Serial.println("Servonum 7 ");
        servonum = 7;
        break ;
      //      case 0xFFA857:
      case 0x807FB24D:
        Serial.println("Repeat - Single Move");  // Return - Single move
        singleMove();
        break ;
      case 0x807F32CD:                        //8 - Servo angle down 10deg
        Serial.println("8 Servo angle down 10deg");
        Serial.print("Servo ");
        Serial.print(servonum);
        Serial.print(" = ");
        Serial.print(finish[servonum]);
        finish[servonum] = moveTable[4][servonum] - 10;
        pwm.setPWM(servonum, 0, pulseWidth(finish[servonum]));
        //        servo[servonum].write(finish[servonum]);
        delay(100);
        moveTable[4][servonum] = finish[servonum];
        Serial.print(" moveTable[4] = ");
        Serial.println(moveTable[4][servonum]);
        break ;
      case 0x807F30Cf:                       //9 - Servo angle up 10deg
        Serial.println("9 Servo angle up 10deg");
        Serial.print("Servo ");
        Serial.print(servonum);
        Serial.print(" = ");
        Serial.print(finish[servonum]);
        finish[servonum] = moveTable[4][servonum] + 10;
        pwm.setPWM(servonum, 0, pulseWidth(finish[servonum]));
        delay(100);
        moveTable[4][servonum] = finish[servonum];
        Serial.print(" moveTable[4] = ");
        Serial.println(moveTable[4][servonum]);
        break ;
      case 0x807F42BD:                      // Fast Rewind - stepSize Down
        Serial.print("Tweak Variables - stepSize Down to ");
        stepSize--;
        Serial.println(stepSize);
        break ;
      case 0x807F02FD:                      // Fast Forward - stepSize Up
        Serial.print("Tweak Variables - stepSize Up to ");
        stepSize++;
        Serial.println(stepSize);
        break ;
      case 0x807F00FF:                      // Chapter Rewind - liftSize Down
        Serial.print("Tweak Variables - liftSize Down to ");
        liftSize--;
        Serial.println(liftSize);
        break ;
      case 0x807FC03F:                      // Chapter Forward - liftSize Up
        Serial.print("Tweak Variables - liftSize Up to ");
        liftSize++;
        Serial.println(liftSize);
        break ;
      case 0x807F58A7:                     // OK - Re-write walkTable
        Serial.println("Re-write walkTable");
        fillwalkTable();
        break ;
      case 0x807F609F:
        Serial.println("Walk");           // Up Arrow Walk  ********************************
        for (int steps = 0; steps < stepCount; steps++) {   // Number of steps to take
          Walk(0);
        }
        break ;
      case 0x807F8877:                    // Volume Up stepCount up
        Serial.print("Tweak Variables - Steps Up to ");
        stepCount++;
        Serial.println(stepCount);
        break ;
      case 0x807F8A75:                    // Volume down stepCount down
        Serial.print("Tweak Variables - Steps Down to ");
        stepCount--;
        Serial.println(stepCount);
        break ;
      case 0xFF4AB5:                        //Down Arrow
        Serial.println("Down Arrow. ");
        Serial.print("Servo ");
        Serial.print(servonum);
        Serial.print(" = ");
        Serial.print(finish[servonum]);
        moveTable[4][servonum] = moveTable[4][servonum] - 10;
        pwm.setPWM(servonum, 0, pulseWidth(finish[servonum]));
        //        servo[servonum].write(finish[servonum]);
        delay(100);
        moveTable[4][servonum] = finish[servonum];
        Serial.print(" srv_pos = ");
        Serial.println(moveTable[4][servonum]);
        break ;
      case 0xFF18E7:                       //Up Arrow
        Serial.println("Up Arrow. ");
        Serial.print("Servo ");
        Serial.print(servonum);
        Serial.print(" = ");
        Serial.print(finish[servonum]);
        finish[servonum] = moveTable[4][servonum] + 10;
        pwm.setPWM(servonum, 0, pulseWidth(finish[servonum]));
        delay(100);
        moveTable[4][servonum] = finish[servonum];
        Serial.print(" srv_pos = ");
        Serial.println(moveTable[4][servonum]);
        break ;
        case 0x807F6897:                     // Down Arrow - Reverse  *********************
        Serial.println("Down Arrow - Reverse");
        doMove(2);
        /*
         * for (int steps = 0; steps < stepCount; steps++) {   // Number of steps to take
          Walk(1);
        }
      */
        break ;


      case 0x807F5AA5:                     // Left Arrow - Turn left  *********************
        Serial.println("Left Arrow - Turn Left");
        for (int steps = 0; steps < stepCount; steps++) {   // Number of steps to take
          doMove(1);
          //   Walk(3);
        }
        break ;
      case 0x807FD827:                     // Right Arrow - Turn Right  *********************
        Serial.println("Right Arrow - Turn Right");
        //for (int steps = 0; steps < stepCount; steps++) {   // Number of steps to take
        //  Walk(2);
        doMove(3);
        break ;
      case 0x807FB04F:                    // Display   -  Shake ************************************
        Serial.print("Display - Shake");
        doMove(2);                        // Sit
        delay(500);
        moveTable[4][4] = 180;
        doMove(4);
        //        pwm.setPWM(4, 0, pulseWidth(180));
        delay(500);
        moveTable[4][0] = 0;
        doMove(4);
        //        pwm.setPWM(0, 0, pulseWidth(0));
        delay(200);
        moveTable[4][4] = 180;
        doMove(4);
        pwm.setPWM(0, 0, pulseWidth(30));
        delay(600);
        pwm.setPWM(0, 0, pulseWidth(0));
        delay(200);
        pwm.setPWM(0, 0, pulseWidth(30));
        delay(1000);
        doMove(2);                        // Sit
        break ;
      case 0x807FC837:                     // Clear - Reset Servo Adjusts
        Serial.println("Clear - Reset Servo Adjusts");
        for (int p = 0; p < numServos; p++) {
          legAdjust[p] = 0;
        }
        doMove(3);
        break ;
      case 0x807FAA55:                     // Setup - Reset moveNum
        Serial.println("Setup - Reset moveNum");
        moveNum = 0;
        Serial.print("moveNum = ");
        Serial.print(moveNum);
        Serial.println(" ");
        break ;
      case 0x807FA857:                     // Return - Reset to walk start
        Serial.println("Return - Reset to walk start");
        moveNum = 0;
        stepCount = 1;
        singleMove();
        break ;
      case 0x807F52AD:                      // Red - Upper rear legs start back
        Serial.println("Tweak - Upper rear legs start back");
        tweakVariables(6, 1);
        break ;
      case 0x807F629D:                      // Audio - Lower rear leg start back
        Serial.println("Audio - Lower rear legs start back");
        tweakVariables(2, 1);
        break ;
      case 0x807F12ED:                      // Green - Upper rear legs start forward
        Serial.println("Tweak Variables - Upper rear legs start forward");
        tweakVariables(6, 0);
        break ;
      case 0x807F22DD:                      // Sub-T - Lower rear legs start forward
        Serial.println("Tweak Variables - Lower rear legs start forward");
        tweakVariables(2, 0);
        break ;
      case 0x807F10EF:                      // Yellow - Upper front legs start back
        Serial.println("Tweak Variables - Upper front legs start back");
        tweakVariables(4, 1);
        break ;
      case 0x807F20DF:                      // Stop - Lower front legs start back
        Serial.println("Tweak Variables - Lower front legs start back");
        tweakVariables(0, 1);
        break ;
      case 0x807FD02F:                      // Blue - Upper front legs start forward
        Serial.println("Tweak Variables - Upper front legs start forward");
        tweakVariables(4, 0);
        break ;
      case 0x807FE01F:                      // Play - Lower front legs start forward
        Serial.println("Tweak Variables - Lower front legs start forward");
        tweakVariables(0, 0);
        break ;
    }
  }
  delay(irDelay);
  irrecv.resume(); // Receive the next value
}

//************************************************************************
int displayResults(int move) {
  //   int s = 0;
  Serial.print("Servo  ");
  for (int s = 0; s < numServos; s++) {
    Serial.print("\t");
    Serial.print(s);
  }
  Serial.println(" ");
  Serial.print("Angle");
  for (int s = 0; s < numServos; s++) {
    Serial.print("\t");
    Serial.print(moveTable[4][s]);
    Serial.print(", ");
  }
  Serial.println("  ");
  Serial.print("Adjust ");
  for (int s = 0; s < numServos; s++) {
    Serial.print("\t");
    Serial.print(legAdjust[s]);
    Serial.print(", ");
  }
  Serial.println(" ");
  Serial.println(" ");
}
//************************************************************************
int allDone() {                // Check if all servos have reached their finish position. Not used during Walk.
  int c = 0;
  for (int d = 0; d < numServos; d++)   {
    if (done[d] > 0)
      c++;
  }
  if (c == numServos) {
    for (int d = 0; d < numServos; d++) {
      done[d] = 0;
    }
  }
  return c;
}
