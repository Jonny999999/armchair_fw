#include "types.hpp"
extern "C"
{
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "freertos/queue.h"

//custom C libraries
#include "wifi.h"
}

#include "config.hpp"
#include "control.hpp"
#include "uart.hpp"


//used definitions moved from config.hpp:
//#define JOYSTICK_TEST


//tag for logging
static const char * TAG = "control";
const char* controlModeStr[7] = {"IDLE", "JOYSTICK", "MASSAGE", "HTTP", "MQTT", "BLUETOOTH", "AUTO"};


//FIXME controlledMotor class not available for this pcb, rework
  //-----------------------------
  //-------- constructor --------
  //-----------------------------
  controlledArmchair::controlledArmchair (
          control_config_t config_f,
          buzzer_t * buzzer_f,
          evaluatedJoystick* joystick_f,
          httpJoystick* httpJoystick_f
          ){
  
      //copy configuration
      config = config_f;
      //copy object pointers
      buzzer = buzzer_f;
      joystick_l = joystick_f,
      httpJoystickMain_l = httpJoystick_f;
      //set default mode from config
      modePrevious = config.defaultMode;
      
      //TODO declare / configure controlled motors here instead of config (unnecessary that button object is globally available - only used here)?
  }
  
  
  
  //----------------------------------
  //---------- Handle loop -----------
  //----------------------------------
  //function that repeatedly generates motor commands depending on the current mode
  //also handles fading and current-limit
  void controlledArmchair::startHandleLoop() {
      while (1){
          ESP_LOGV(TAG, "control task executing... mode=%s", controlModeStr[(int)mode]);
  
          switch(mode) {
              default:
                  mode = controlMode_t::IDLE;
                  break;
  
              case controlMode_t::IDLE:
				  //send both motors idle command to motorctl pcb
				  uart_sendStruct<motorCommands_t>(cmds_bothMotorsIdle);
				  commands_now = cmds_bothMotorsIdle;
                  vTaskDelay(200 / portTICK_PERIOD_MS);
  #ifdef JOYSTICK_LOG_IN_IDLE
  				//get joystick data here (without using it)
  				//since loglevel is DEBUG, calculateion details is output
                  joystick_l->getData(); //get joystick data here
  #endif
                  break;
  
  
              case controlMode_t::JOYSTICK:
                  vTaskDelay(50 / portTICK_PERIOD_MS);
                  //get current joystick data with getData method of evaluatedJoystick
                  stickData = joystick_l->getData();
                  //additionaly scale coordinates (more detail in slower area)
                  joystick_scaleCoordinatesLinear(&stickData, 0.6, 0.35); //TODO: add scaling parameters to config
                  //generate motor commands
                  commands_now = joystick_generateCommandsDriving(stickData, altStickMapping);
                  //apply motor commands
				  uart_sendStruct<motorCommands_t>(commands_now);
                  break;
  
  
              case controlMode_t::MASSAGE:
                  vTaskDelay(10 / portTICK_PERIOD_MS);
                  //--- read joystick ---
                  //only update joystick data when input not frozen
                  if (!freezeInput){
                      stickData = joystick_l->getData();
                  }
                  //--- generate motor commands ---
                  //pass joystick data from getData method of evaluatedJoystick to generateCommandsShaking function
                  commands_now = joystick_generateCommandsShaking(stickData);
                  //apply motor commands
				  uart_sendStruct<motorCommands_t>(commands_now);
                  break;
  
  
              case controlMode_t::HTTP:
                  //--- get joystick data from queue ---
                  //Note this function waits several seconds (httpconfig.timeoutMs) for data to arrive, otherwise Center data or NULL is returned
                  //TODO: as described above, when changing modes it might delay a few seconds for the change to apply
                  stickData = httpJoystickMain_l->getData();
                  //scale coordinates additionally (more detail in slower area)
                  joystick_scaleCoordinatesLinear(&stickData, 0.6, 0.4); //TODO: add scaling parameters to config
                  ESP_LOGD(TAG, "generating commands from x=%.3f  y=%.3f  radius=%.3f  angle=%.3f", stickData.x, stickData.y, stickData.radius, stickData.angle);
                  //--- generate motor commands ---
                  //Note: timeout (no data received) is handled in getData method
                  commands_now = joystick_generateCommandsDriving(stickData, altStickMapping);
  
                  //--- apply commands to motors ---
				  uart_sendStruct<motorCommands_t>(commands_now);
                 break;
  
  
              case controlMode_t::AUTO:
				 //FIXME auto mode currently not supported, needs rework
                  vTaskDelay(20 / portTICK_PERIOD_MS);
//                 //generate commands
//                 commands_now = armchair.generateCommands(&instruction);
//                  //--- apply commands to motors ---
//                  //TODO make motorctl.setTarget also accept motorcommand struct directly
//				  uart_sendStruct<motorCommands_t>(commands_now);
//                 //motorRight->setTarget(commands_now.right.state, commands_now.right.duty); 
//                 //motorLeft->setTarget(commands_now.left.state, commands_now.left.duty); 
//  
//                 //process received instruction
//                 switch (instruction) {
//                     case auto_instruction_t::NONE:
//                         break;
//                     case auto_instruction_t::SWITCH_PREV_MODE:
//                         toggleMode(controlMode_t::AUTO);
//                         break;
//                     case auto_instruction_t::SWITCH_JOYSTICK_MODE:
//                         changeMode(controlMode_t::JOYSTICK);
//                         break;
//                     case auto_instruction_t::RESET_ACCEL_DECEL:
////                         //enable downfading (set to default value)
////                         motorLeft->setFade(fadeType_t::DECEL, true);
////                         motorRight->setFade(fadeType_t::DECEL, true);
////                         //set upfading to default value
////                         motorLeft->setFade(fadeType_t::ACCEL, true);
////                         motorRight->setFade(fadeType_t::ACCEL, true);
////                         break;
//                     case auto_instruction_t::RESET_ACCEL:
////                         //set upfading to default value
////                         motorLeft->setFade(fadeType_t::ACCEL, true);
////                         motorRight->setFade(fadeType_t::ACCEL, true);
////                         break;
//                     case auto_instruction_t::RESET_DECEL:
////                         //enable downfading (set to default value)
////                         motorLeft->setFade(fadeType_t::DECEL, true);
////                         motorRight->setFade(fadeType_t::DECEL, true);
//                         break;
//                 }
                 break;
  
                //TODO: add other modes here
          }
  
  
          //--- run actions based on received button button event ---
  		//note: buttonCount received by sendButtonEvent method called from button.cpp
          //TODO: what if variable gets set from other task during this code? -> mutex around this code
		  //TODO add methods and move below code to button file possible?
          switch (buttonCount) {
              case 1: //define joystick center or freeze input
                  if (mode == controlMode_t::JOYSTICK){
                      //joystick mode: calibrate joystick
                      joystick_l->defineCenter();
					  buzzer->beep(2, 50, 30);
					  buzzer->beep(1, 200, 25);
                  } else if (mode == controlMode_t::MASSAGE){
                      //massage mode: toggle freeze of input (lock joystick at current values)
                      freezeInput = !freezeInput;
                      if (freezeInput){
                          buzzer->beep(5, 40, 25);
                      } else {
                          buzzer->beep(1, 300, 100);
                      }
                  }
                  break;
  
              case 12: //toggle alternative joystick mapping (reverse swapped) 
                  altStickMapping = !altStickMapping;
                  if (altStickMapping){
                      buzzer->beep(6, 70, 50);
                  } else {
                      buzzer->beep(1, 500, 100);
                  }
                  break;
          }
          //--- reset button event --- (only one action per run)
          if (buttonCount > 0){
              ESP_LOGI(TAG, "resetting button event/count");
              buttonCount = 0;
          }
  
  
  
          //-----------------------
          //------ slow loop ------
          //-----------------------
          //this section is run about every 5s (+500ms)
          if (esp_log_timestamp() - timestamp_SlowLoopLastRun > 5000) {
              ESP_LOGV(TAG, "running slow loop... time since last run: %.1fs", (float)(esp_log_timestamp() - timestamp_SlowLoopLastRun)/1000);
              timestamp_SlowLoopLastRun = esp_log_timestamp();
  
              //run function which detects timeout (switch to idle)
              handleTimeout();
          }
  
      }//end while(1)
  }//end startHandleLoop
  
  
  
  //-----------------------------------
  //---------- resetTimeout -----------
  //-----------------------------------
  void controlledArmchair::resetTimeout(){
      //TODO mutex
      timestamp_lastActivity = esp_log_timestamp();
  }
  
  
  
  //------------------------------------
  //--------- sendButtonEvent ----------
  //------------------------------------
  void controlledArmchair::sendButtonEvent(uint8_t count){
      //TODO mutex - if not replaced with queue
      ESP_LOGI(TAG, "setting button event");
      buttonCount = count;
  }
  
  
  
  //------------------------------------
  //---------- handleTimeout -----------
  //------------------------------------
  //percentage the duty can vary since last timeout check and still counts as incative 
  //TODO: add this to config
  float inactivityTolerance = 10; 
  
  //local function that checks whether two values differ more than a given tolerance
  bool validateActivity(float dutyOld, float dutyNow, float tolerance){
      float dutyDelta = dutyNow - dutyOld;
      if (fabs(dutyDelta) < tolerance) {
          return false; //no significant activity detected
      } else {
          return true; //there was activity
      }
  }
  
  //function that evaluates whether there is no activity/change on the motor duty for a certain time. If so, a switch to IDLE is issued.
  //has to be run repeatedly in a *slow interval* so change between current and last duty is detectable
  void controlledArmchair::handleTimeout(){
      //check for timeout only when not idling already
      if (mode != controlMode_t::IDLE) {
		  //activity detected between current and last generated motor commands
		  if (validateActivity(commands_lastActivityCheck.left.duty, commands_now.left.duty, inactivityTolerance) 
				  || validateActivity(commands_lastActivityCheck.right.duty, commands_now.right.duty, inactivityTolerance)
					 ){
              ESP_LOGD(TAG, "timeout check: [activity] detected since last check -> reset");
              //reset last commands and timestamp
			  commands_lastActivityCheck = commands_now;
              resetTimeout();
          }
          //no activity on any motor and msTimeout exceeded
          else if (esp_log_timestamp() - timestamp_lastActivity > config.timeoutMs){
              ESP_LOGW(TAG, "timeout check: [TIMEOUT], no activity for more than %.ds  -> switch to idle", config.timeoutMs/1000);
              //toggle to idle mode
              toggleIdle();
          }
          else {
              ESP_LOGD(TAG, "timeout check: [inactive], last activity %.1f s ago, timeout after %d s", (float)(esp_log_timestamp() - timestamp_lastActivity)/1000, config.timeoutMs/1000);
          }
      }
  }
  
  
  
  //-----------------------------------
  //----------- changeMode ------------
  //-----------------------------------
  //function to change to a specified control mode
  //FIXME FIXME: replace change with motorLeft object with update config via uart
  void controlledArmchair::changeMode(controlMode_t modeNew) {
      //reset timeout timer
      resetTimeout();
  
      //exit if target mode is already active
      if (mode == modeNew) {
          ESP_LOGE(TAG, "changeMode: Already in target mode '%s' -> nothing to change", controlModeStr[(int)mode]);
          return;
      }
  
      //copy previous mode
      modePrevious = mode;
  
  	ESP_LOGW(TAG, "=== changing mode from %s to %s ===", controlModeStr[(int)mode], controlModeStr[(int)modeNew]);
  
  	//========== commands change FROM mode ==========
  	//run functions when changing FROM certain mode
  	switch(modePrevious){
  		default:
  			ESP_LOGI(TAG, "noting to execute when changing FROM this mode");
  			break;
  
  #ifdef JOYSTICK_LOG_IN_IDLE
  		case controlMode_t::IDLE:
  			ESP_LOGI(TAG, "disabling debug output for 'evaluatedJoystick'");
  			esp_log_level_set("evaluatedJoystick", ESP_LOG_WARN); //FIXME: loglevel from config
  			break;
  #endif
  
  		case controlMode_t::HTTP:
  			ESP_LOGW(TAG, "switching from http mode -> disabling http and wifi");
  			//stop http server
  			ESP_LOGI(TAG, "disabling http server...");
  			http_stop_server();
  
  			//FIXME: make wifi function work here - currently starting wifi at startup (see notes main.cpp)
              //stop wifi
              //TODO: decide whether ap or client is currently used - which has to be disabled?
              //ESP_LOGI(TAG, "deinit wifi...");
              //wifi_deinit_client();
              //wifi_deinit_ap();
              ESP_LOGI(TAG, "done stopping http mode");
              break;
  
//          case controlMode_t::MASSAGE:
//              ESP_LOGW(TAG, "switching from MASSAGE mode -> restoring fading, reset frozen input");
//              //TODO: fix issue when downfading was disabled before switching to massage mode - currently it gets enabled again here...
//              //enable downfading (set to default value)
//              motorLeft->setFade(fadeType_t::DECEL, true);
//              motorRight->setFade(fadeType_t::DECEL, true);
//              //set upfading to default value
//              motorLeft->setFade(fadeType_t::ACCEL, true);
//              motorRight->setFade(fadeType_t::ACCEL, true);
//              //reset frozen input state
//              freezeInput = false;
//              break;
//  
//          case controlMode_t::AUTO:
//              ESP_LOGW(TAG, "switching from AUTO mode -> restoring fading to default");
//              //TODO: fix issue when downfading was disabled before switching to auto mode - currently it gets enabled again here...
//              //enable downfading (set to default value)
//              motorLeft->setFade(fadeType_t::DECEL, true);
//              motorRight->setFade(fadeType_t::DECEL, true);
//              //set upfading to default value
//              motorLeft->setFade(fadeType_t::ACCEL, true);
//              motorRight->setFade(fadeType_t::ACCEL, true);
//              break;
      }
  
  
      //========== commands change TO mode ==========
      //run functions when changing TO certain mode
      switch(modeNew){
          default:
              ESP_LOGI(TAG, "noting to execute when changing TO this mode");
              break;
  
  		case controlMode_t::IDLE:
  			buzzer->beep(1, 1500, 0);
  #ifdef JOYSTICK_LOG_IN_IDLE
  			esp_log_level_set("evaluatedJoystick", ESP_LOG_DEBUG);
  #endif
  			break;
  
          case controlMode_t::HTTP:
              ESP_LOGW(TAG, "switching to http mode -> enabling http and wifi");
              //start wifi
              //TODO: decide wether ap or client should be started
              ESP_LOGI(TAG, "init wifi...");
  
              //FIXME: make wifi function work here - currently starting wifi at startup (see notes main.cpp)
              //wifi_init_client();
              //wifi_init_ap();
  
              //wait for wifi
              //ESP_LOGI(TAG, "waiting for wifi...");
              //vTaskDelay(1000 / portTICK_PERIOD_MS);
  
              //start http server
              ESP_LOGI(TAG, "init http server...");
              http_init_server();
              ESP_LOGI(TAG, "done initializing http mode");
              break;
  
//          case controlMode_t::MASSAGE:
//              ESP_LOGW(TAG, "switching to MASSAGE mode -> reducing fading");
//              uint32_t shake_msFadeAccel = 500; //TODO: move this to config
//  
//              //disable downfading (max. deceleration)
//              motorLeft->setFade(fadeType_t::DECEL, false);
//              motorRight->setFade(fadeType_t::DECEL, false);
//              //reduce upfading (increase acceleration)
//              motorLeft->setFade(fadeType_t::ACCEL, shake_msFadeAccel);
//              motorRight->setFade(fadeType_t::ACCEL, shake_msFadeAccel);
//              break;
  
      }
  
      //--- update mode to new mode ---
      //TODO: add mutex
      mode = modeNew;
  }
  
  
  //TODO simplify the following 3 functions? can be replaced by one?
  
  //-----------------------------------
  //----------- toggleIdle ------------
  //-----------------------------------
  //function to toggle between IDLE and previous active mode
  void controlledArmchair::toggleIdle() {
      //toggle between IDLE and previous mode
      toggleMode(controlMode_t::IDLE);
  }
  
  
  
  //------------------------------------
  //----------- toggleModes ------------
  //------------------------------------
  //function to toggle between two modes, but prefer first argument if entirely different mode is currently active
  void controlledArmchair::toggleModes(controlMode_t modePrimary, controlMode_t modeSecondary) {
      //switch to secondary mode when primary is already active
      if (mode == modePrimary){
          ESP_LOGW(TAG, "toggleModes: switching from primaryMode %s to secondarMode %s", controlModeStr[(int)mode], controlModeStr[(int)modeSecondary]);
          buzzer->beep(2,200,100);
          changeMode(modeSecondary); //switch to secondary mode
      } 
      //switch to primary mode when any other mode is active
      else {
          ESP_LOGW(TAG, "toggleModes: switching from %s to primary mode %s", controlModeStr[(int)mode], controlModeStr[(int)modePrimary]);
          buzzer->beep(4,200,100);
          changeMode(modePrimary);
      }
  }
  
  
  
  //-----------------------------------
  //----------- toggleMode ------------
  //-----------------------------------
  //function that toggles between certain mode and previous mode
  void controlledArmchair::toggleMode(controlMode_t modePrimary){
  
      //switch to previous mode when primary is already active
      if (mode == modePrimary){
          ESP_LOGW(TAG, "toggleMode: switching from primaryMode %s to previousMode %s", controlModeStr[(int)mode], controlModeStr[(int)modePrevious]);
          //buzzer->beep(2,200,100);
          changeMode(modePrevious); //switch to previous mode
      } 
      //switch to primary mode when any other mode is active
      else {
          ESP_LOGW(TAG, "toggleModes: switching from %s to primary mode %s", controlModeStr[(int)mode], controlModeStr[(int)modePrimary]);
          //buzzer->beep(4,200,100);
          changeMode(modePrimary);
      }
  }
