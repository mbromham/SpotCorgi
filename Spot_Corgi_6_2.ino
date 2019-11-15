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
int walkRate = 7;                           // Determine pace of servo moves during Walk
int stepCount = 3;                          // How many steps to take in Walk or moves in singleMove
int irDelay = 150;                          // Delay for IR reads
int lowerStart = 100;                       // Servo 0 start
int upperStart = 0;                         // Servo 4 start
int liftSize = 30;                          // Lower leg raise distance
int stepSize = 30;                          // Upper leg move distance. Array fill crashes if > 48. Don't know why.
int moveCount = 0;                          // Number of moves in walkTable. Defined during array fill.
int moveNum = 0;                            // Move counter for singleMove
int usCountdown;                            // Upper start countdown holder for moves to return to start
float legArc[100];                          // Holder for circular function to adjust lower leg angle to keep body height constant as upper leg moves back (currently disabled for testing)
int legStartDelay;                          // Moves before a specific leg steps. Defined as fraction of moveCount, usually 1/4.
int walkTable[200][8] = {lowerStart, 0, 0, 0, upperStart, 0, 0, 0};  // Array of positions for L & U (0 & 4). Not all 100 are used but need to be specified to reserve memory.

String moves[6] = {"Start", "Crouch", "Sit", "Stand", "Shake", "Stand Backup"};
//                    0   1    2    3  4  5  6  7
int legAdjust[8] = {  0, 0, -20, -20, 0, 0, -20, -20};            // Adjust walk angle for each leg
int srv_pos[8]   = {101, 79, 101, 81, 29, 151, 179, 1};   // Current servo position placeholder
int moveTable[6][8]  = {                                  //Servo settings for preset positions
  // 0,   1,   2,   3,  4,   5,  6,  7
  {101,  81,  81, 101,  1, 158, 158, 3},    //Start
  {180,   0,   0, 180,  0, 180, 180, 0},    //Crouch   Down Arrow
  { 45, 135,  45, 135, 10, 170, 170, 10},   //Sit      Left Arrow
  {120, 80, 100, 80,  0, 180, 180,  0},   //Stand    Right Arrow
  { 35, 155,  35, 155, 160, 130, 170, 0},   //Shake    Display
  {120, 80, 100, 80,  0, 180, 180,  0}    //Stand values backup for reset
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
  Serial.println("Write walk move array");
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
//************************************************************************
int tweakVariables(int serv, int fb) {       // Adjust walk variables using remote
  int tweaks[3][8] =
  { { 1,  1,  1,  1,  1,  1,  1,  1},                  // Move leg walk start Forward
    {-1, -1, -1, -1, -1, -1, -1, -1},                  // Move leg walk start Backward
    { 1, -1, -1,  1,  1, -1, -1,  1}                   // Adjust for Left/Right Side
  };
  for (int i = 0; i < 2; i++) {              // Set servos
    moveTable[3][i + serv] = moveTable[3][i + serv]  + tweaks[fb][i + serv];
    legAdjust[i + serv] = legAdjust[i + serv] + tweaks[fb][i + serv];
    moveTable[3][i + serv] = moveTable[5][i + serv] + (legAdjust[i + serv]* tweaks[2][i + serv]);
  }
    Serial.print("       ");
  for (int i = 0; i < numServos; i++) {               // Display adjust values
    Serial.print(legAdjust[i]);
    Serial.print(", ");
  }
  doMove(3);
  Serial.println(" ");
}
// ************************************************************************
int fillwalkTable()                                    // Writes array of positions all servos to Walk
{
  walkTable[0][0] = lowerStart;                        // Set start, lift & move step values for servos 0 & 4
  walkTable[0][4] = upperStart;
  legArc[0] = 0;
  walkTable[1][0] = lowerStart + liftSize;
  walkTable[1][4] = upperStart;
  legArc[1] = 0;
  walkTable[2][0] = lowerStart + liftSize;
  walkTable[2][4] = upperStart + stepSize;
  legArc[2] = 0;
  walkTable[3][0] = lowerStart + liftSize;
  walkTable[3][4] = upperStart + stepSize;
  legArc[3] = 0;
  usCountdown = walkTable[3][4];
  moveCount  = 4;
  do {                                                 // Complete the rest of the moves for 0 & 4 counting down Upper to start position.
    usCountdown--;
    walkTable[moveCount][0] = lowerStart;
    walkTable[moveCount][4] = usCountdown;
    moveCount++;
    moveNum++;
  }
  while (usCountdown > upperStart + 1);
  for (int m = 4; m < moveCount; m++) {               // Rebuild array to adjust lower leg angle to keep body height constant as upper leg moves back (currently disabled for testing)
    //legArc[m] = sqrt(pow(((moveCount - 4) / 2), 2) - pow((((moveCount - 4) / 2) - m + 4), 2));
    legArc[m] = 0;
  }

  for (int m = 0; m < moveCount; m++) {               // Duplicate 0 & 4 array to allow for delayed start of each leg
    walkTable[moveCount + m][0] = walkTable[m][0];
    walkTable[moveCount + m][4] = walkTable[m][4];
  }

  for (int m = 0; m < moveCount; m++) {               // Write out full Walk array for all servos including staggered starts and angle adjustments
    legStartDelay = moveCount / 4;                    // Moves to wait before starting a specific leg
    /*
        //                                                                                         Leg start delays bypassed
        walkTable[m][0] = walkTable[m][0] + legAdjust[0] + legArc[m];
        walkTable[m][1] = 180 - (walkTable[m][0] + legAdjust[1] + legArc[m]);
        walkTable[m][2] = 180 - (walkTable[m][0] - legAdjust[2] + legArc[m]);
        walkTable[m][3] = walkTable[m][0] + legAdjust[3] + legArc[m];
        walkTable[m][4] = walkTable[m][4] + legAdjust[4];
        walkTable[m][5] = 180 - (walkTable[m][4] + legAdjust[5]);
        walkTable[m][6] = 180 - (walkTable[m][4] + legAdjust[6]);
        walkTable[m][7] = walkTable[m][4] + legAdjust[7];
    */

    //                                                                                       Legs start delays included
    walkTable[m][0] = walkTable[m][0] + legAdjust[0] + legArc[m];
    walkTable[m][1] = 180 - (walkTable[m + (legStartDelay)][0] + legAdjust[1] + legArc[m]);
    walkTable[m][2] = 180 - (walkTable[m + (legStartDelay * 3)][0] + legAdjust[2] + legArc[m]);
    walkTable[m][3] = walkTable[m + (legStartDelay * 2)][0] + legAdjust[3] + legArc[m];
    walkTable[m][4] = walkTable[m][4] + legAdjust[4];
    walkTable[m][5] = 180 - (walkTable[m + (legStartDelay)][4] + legAdjust[5]);
    walkTable[m][6] = 180 - (walkTable[m + (legStartDelay * 3)][4] + legAdjust[6]);
    walkTable[m][7] = walkTable[m + (legStartDelay * 2)][4] + legAdjust[7];
  }
  Serial.println(" ");
  Serial.print("Fill walkTable -");
  Serial.print("  moveCount = ");
  Serial.println(moveCount);
  Serial.println("                       0   1   2   3  4    5    6  7");
  //   Serial.println("  ");
  for (int c = 0; c < moveCount; c++) {               // Display array values
    Serial.print("   Move = ");
    Serial.print(c);
    Serial.print(" Servos = ");
    for (int s = 0; s < numServos; s++)  {
      Serial.print(walkTable[c][s]);
      Serial.print(", ");
    }
    Serial.print("legArc =");
    Serial.print(legArc[c]);
    Serial.println(" ");
  }
   delay(500);
  //  displayResults(3);
  moveNum = 0;
  doMove(3);                                         // Move to Stand position
}
//************************************************************************
int Walk() {                                        // Take one step for all legs
  for (int p  = 0; p < moveCount; p++) {
    for (int i  = 0; i < numServos; i++) {
      servonum = i;
      finish[servonum] = walkTable[p][i];
      pwm.setPWM(servonum, 0, pulseWidth(finish[servonum]));
      delay(walkRate);
      srv_pos[servonum] = finish[servonum];
    }
  }
}
//************************************************************************
int PSM(int, int )                        // Paced Servo Move - Move servo from start to finish by 1deg steps
{
  if (srv_pos[servonum] > finish[servonum]) {
    pwm.setPWM(servonum, 0, pulseWidth(srv_pos[servonum]));
    srv_pos[servonum]--;
    delay(baseRate);
  }
  else
  {
    pwm.setPWM(servonum, 0, pulseWidth(srv_pos[servonum]));
    srv_pos[servonum]++;
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
      if (srv_pos[servonum] != finish[servonum])
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
      srv_pos[servonum] = finish[servonum];
      Serial.print("Servonum = ");
      Serial.print(servonum);
      Serial.print(" Finish = ");
      Serial.println(finish[servonum]);
    }
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
      case 0x807F4AB5:
        Serial.println("Servonum 1");
        servonum = 1;
        break;
      case 0x807F0AF5:
        Serial.println("Servonum 2 ");
        servonum = 2;
        break;
      case 0x807F08F7:
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
        finish[servonum] = srv_pos[servonum] - 10;
        pwm.setPWM(servonum, 0, pulseWidth(finish[servonum]));
        //        servo[servonum].write(finish[servonum]);
        delay(100);
        srv_pos[servonum] = finish[servonum];
        Serial.print(" srv_pos = ");
        Serial.println(srv_pos[servonum]);
        break ;
      case 0x807F30Cf:                       //9 - Servo angle up 10deg
        Serial.println("9 Servo angle up 10deg");
        Serial.print("Servo ");
        Serial.print(servonum);
        Serial.print(" = ");
        Serial.print(finish[servonum]);
        finish[servonum] = srv_pos[servonum] + 10;
        pwm.setPWM(servonum, 0, pulseWidth(finish[servonum]));
        delay(100);
        srv_pos[servonum] = finish[servonum];
        Serial.print(" srv_pos = ");
        Serial.println(srv_pos[servonum]);
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
        Serial.print("Re-write walkTable");
        fillwalkTable();
        break ;
      case 0x807F609F:
        Serial.println("Walk");           // Up Arrow Walk  ********************************
        for (int steps = 0; steps < stepCount; steps++) {   // Number of steps to take
          Walk();
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
      case 0x807F6897:                     // Down Arrow - Crouch  *********************
        Serial.println("Down Arrow - Crouch");
        doMove(1);
        break ;
      case 0x807F5AA5:                     // Left Arrow - Sit  *********************
        Serial.println("Left Arrow - Sit");
        doMove(2);
        break ;
      case 0x807FD827:                     // Right Arrow - Stand  *********************
        Serial.println("Right Arrow - Stand");
        doMove(3);
        break ;
      case 0x807FB04F:                    // Display   -  Shake ************************************
        Serial.print("Display - Shake");
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
      case 0x807FC837:                     // Clear - Reset Servo Adjusts
        Serial.println("Clear - Reset Servo Adjusts");
        for (int p = 0; p < 8; p++) {
          legAdjust[p] = 0;
          moveTable[3][p] = moveTable[5][p];
          //Serial.print(moveTable[3][p]);
          //Serial.print(" , ");
        }
        doMove(3);
//        Serial.println(" ");
        break ;
      case 0x807FAA55:                     // Setup - Reset moveNum
        Serial.println("Clear - Reset moveNum");
        moveNum = 0;
        Serial.print("moveNum = ");
        Serial.print(moveNum);
        Serial.println(" ");
        break ;
      case 0x807FA857:                     // Return - Reset to walk start
        Serial.println("Clear - Reset to walk start");
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
    delay(irDelay);
    irrecv.resume(); // Receive the next value
  }
}
//************************************************************************
int displayResults(int move) {
  //   int s = 0;
  Serial.print("Set Servos to: ");
  Serial.println(moves[move]);
  Serial.println("Servo  0   1   2   3  4    5    6  7");
  Serial.print("     ");
  for (int s = 0; s < numServos; s++) {
    Serial.print(srv_pos[s]);
    Serial.print(", ");
  }
  Serial.println(" ");
}
//************************************************************************
int allDone()                 // Check if all servos have reached their finish position. Not used during Walk.
{
  int c = 0;
  for (int d = 0; d < numServos; d++)   {
    if (done[d] > 0)
    {
      c++;
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
