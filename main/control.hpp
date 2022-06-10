#pragma once


//enum that decides how the motors get controlled
enum class controlMode_t {IDLE, JOYSTICK, MASSAGE, MQTT, BLUETOOTH, AUTO};
//extern controlMode_t mode;

//task that repeatedly generates motor commands depending on the current mode
void task_control(void * pvParameters);

//function that changes to a specified control mode
void control_changeMode(controlMode_t modeNew);



