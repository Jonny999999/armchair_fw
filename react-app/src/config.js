//=========================================
//========= web-app configuration =========
//=========================================
// central place for all tuning parameters of the armchair web-app

export const config = {
    //--- driving joystick ---
    joystickSize: 250,        // affects size on screen and scaling of the coordinates
    joystickThrottleMs: 300,  // min interval the joystick sends data while being moved
    decimalPlaces: 3,         // resolution of the coordinates sent to the controller

    //--- heartbeat ---
    // The controller stops the motors when it did not receive any data for a while
    // (safety, e.g. phone lost the connection). Since data is only sent on joystick
    // *events*, just holding the stick still while driving straight used to run into
    // that timeout. -> additionally repeat the last position in this interval.
    // Note: has to be well below 'timeoutMs' of configHttpJoystickMain (config.cpp)
    heartbeatIntervalMs: 1000,

    //--- chair adjustment ---
    restPollIntervalMs: 1000, // interval the actual rest positions are fetched
    restSliderStep: 5,        // step size of the position slider in percent

    //--- settings ---
    // max duty limits the top speed - same setting as 'Set max Duty' in the encoder-menu.
    // Note: the controller stores it in nvs -> only send it when the slider is released
    settingsPollIntervalMs: 5000, // interval the max-duty is fetched (can also be changed at the encoder)
    maxDutyMin: 10,
    maxDutyMax: 100,
    maxDutyStep: 5,
};
