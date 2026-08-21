//===================================
//========= http api client =========
//===================================
// all requests to the esp32 (see common/http.cpp for the endpoints)

// The app is served by the esp32 itself, so requests go to the same host.
// When developing locally ('npm start') point them at the chair instead:
//   REACT_APP_API_HOST=http://192.168.4.1 npm start
const HOST = process.env.REACT_APP_API_HOST || '';

//---------------------------
//--------- postJson --------
//---------------------------
// Note: content-type is 'text/plain' on purpose - with 'application/json' the browser
// sends a CORS preflight (OPTIONS) request first, which the esp32 server does not answer
// https://stackoverflow.com/questions/1256593/why-am-i-getting-an-options-request-instead-of-a-get-request
const postJson = async (path, data) => {
    const response = await fetch(HOST + path, {
        method: 'POST',
        headers: { 'Content-Type': 'text/plain' },
        body: JSON.stringify(data),
    });
    if (!response.ok)
        throw new Error(`POST ${path} failed with status ${response.status}`);
    return response;
};


//---------------------------
//------- drive/joystick ----
//---------------------------
// send joystick coordinates (-1 to 1) to the controller
// note: has to be actual numbers, the controller parses them as numbers (cJSON)
export const sendJoystick = (x, y) => postJson('/api/joystick', { x, y });


//---------------------------
//----- chair adjustment ----
//---------------------------
// rest: 'leg' or 'back'

// start moving in one direction ('up'/'down') or stop ('stop') - used by the hold-buttons
export const sendRestAction = (rest, action) => postJson('/api/chair', { rest, action });

// move the rest to a certain position (0-100%)
// note: sending 0/100 again while already there is not ignored by the controller, it
// re-runs the motor into the limit switch which re-syncs the (time based) tracked position
export const sendRestPercent = (rest, percent) => postJson('/api/chair', { rest, percent });

// current position and state of both rests
export const fetchRestStatus = async () => {
    const response = await fetch(HOST + '/api/chair');
    if (!response.ok)
        throw new Error(`GET /api/chair failed with status ${response.status}`);
    return response.json();
};


//---------------------------
//--------- settings --------
//---------------------------
// max duty in percent (top speed limit, same value as in the encoder-menu)
export const fetchSettings = async () => {
    const response = await fetch(HOST + '/api/settings');
    if (!response.ok)
        throw new Error(`GET /api/settings failed with status ${response.status}`);
    return response.json();
};

// note: the controller writes this to nvs flash -> only send when actually changed
// (e.g. when the slider is released), not on every intermediate value while dragging
export const sendMaxDuty = (maxDuty) => postJson('/api/settings', { maxDuty });
