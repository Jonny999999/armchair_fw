#pragma once

typedef enum {
    REST_OFF = 0,
    REST_DOWN = -1,
    REST_UP = 1
} restState_t;

//Set direction functions for leg-rest
void setLegrestUp();
void setLegrestDown();
void setLegrestOff();

//Set direction functions for back-rest
void setBackrestUp();
void setBackrestDown();
void setBackrestOff();


//Run leg-rest with target direction/state
// 0 = OFF;  <0 = DOWN;  >0 = UP
void runLegrest(float targetDirection);
void runLegrest(restState_t targetState);

//Run back-rest with target direction/state
// 0 = OFF;  <0 = DOWN;  >0 = UP
void runBackrest(float targetDirection);
void runBackrest(restState_t targetState);
