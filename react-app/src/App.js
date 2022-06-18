import { Joystick } from 'react-joystick-component';
import React, { useState} from 'react';
//import { w3cwebsocket as W3CWebSocket } from "websocket";




function App() {
    //declare variables that can be used and updated in html
    const [angle_html, setAngle_html] = useState(0);
    const [x_html, setX_html] = useState(0);
    const [y_html, setY_html] = useState(0);
    const [ip, setIp] = useState("10.0.0.66");
    const [radius_html, setRadius_html] = useState(0);



    //===============================
    //=========== config ============
    //===============================
    const decimalPlaces = 3;
    const joystickSize = 200; //affects scaling of coordinates and size of joystick on website
    const throttle = 300; //throtthe interval the joystick sends data while moving (ms)
    const toleranceSnapToZeroPer = 20;//percentage of moveable range the joystick can be moved from the axix and value stays at 0



    //-------------------------------------------------
    //------- Scale coordinate, apply tolerance -------
    //-------------------------------------------------
    //function that:
    // - scales the coodinate to a range of -1 to 1
    // - snaps 0 zero for a given tolerance in percent
    // - rounds value do given decimal places
    // - TODO: add threshold it snaps to 1 / -1 (100%) toleranceEnd
    const ScaleCoordinate = (input) => {
        //calc tolerance threshold and available range
        const tolerance = joystickSize/2 * toleranceSnapToZeroPer/100;
        const range = joystickSize/2 - tolerance;
        let result = 0;

        //console.log("value:",input,"tolerance:",tolerance,"   range:",range);

        //input positive and above 'snap to zero' threshold
        if ( input > 0 && input > tolerance ){
            result = ((input-tolerance)/range).toFixed(decimalPlaces);
        }
        //input negative and blow 'snap to zero' threshold
        else if ( input < 0 && input < -tolerance ){
            result = ((input+tolerance)/range).toFixed(decimalPlaces);
        }
        //inside threshold around zero
        else {
            result = 0;
        }

        //return result
        //console.log("result:", result, "\n");
        return result;
    }




    //-------------------------------------------
    //------- Senda data via POST request -------
    //-------------------------------------------
    //function that sends an object as json to the esp32 with a http post request
    const httpSendObject = async (object_data) => {
        //debug log
        console.log("Sending:", object_data);

        let json = JSON.stringify(object_data);
        //console.log("json string:", json);
        //remove quotes around numbers:
        //so cJSON parses the values as actua[l numbers than strings
        const regex2 = /"(-?[0-9]+\.{0,1}[0-9]*)"/g
        json = json.replace(regex2, '$1')
        //console.log("json removed quotes:", json);

        //--- API  url / ip ---
        //await fetch("http://10.0.1.69/api/joystick", {
        //await fetch("http://10.0.1.72/api/joystick", {
        await fetch("api/joystick", {
            method: "POST",
            //apparently browser sends OPTIONS request before actual POST request, this OPTIONS request was not handled by esp32
            //also the custom set Access-Control-Allow-Origin header in esp32 url header was not read because of that
            //changed content type to text/plain to workaround this
            //https://stackoverflow.com/questions/1256593/why-am-i-getting-an-options-request-instead-of-a-get-request
            headers: {
                //"Content-Type": "application/json",
                "Content-Type": "text/plain",
            },
            body: json,
        })
            //.then((response) => console.log(response));
    };




    //---------------------------------------
    //--- function when joystick is moved ---
    //---------------------------------------
    //function that is run for each move event
    //evaluate coordinates and send to esp32
    const handleMove = (e) => {
        //console.log("data from joystick-element X:" + e.x + " Y:" + e.y + " distance:" + e.distance);
        //calculate needed variables
        const x = ScaleCoordinate(e.x);
        const y = ScaleCoordinate(e.y);
        const radius = (e.distance / 100).toFixed(5);
        const angle = ( Math.atan( y / x ) * 180 / Math.PI ).toFixed(2);

        //crate object with necessary data
        const joystick_data={
            x: x,
            y: y,
            radius: radius, 
            angle: angle
        }
        //send object with joystick data as json to controller
        httpSendObject(joystick_data);

        //update variables for html
        setX_html(joystick_data.x);
        setY_html(joystick_data.y);
        setRadius_html(joystick_data.radius);
        setAngle_html(joystick_data.angle);
    };



    //------------------------------------------
    //--- function when joystick is released ---
    //------------------------------------------
    const handleStop = (e) => {
        //create object with all values 0
        const joystick_data={
            x: 0,
            y: 0,
            radius: 0,
            angle: 0
        }

        //update variables for html
        setX_html(0);
        setY_html(0);
        setRadius_html(0);
        setAngle_html(0);
        //send object with joystick data as json to controller
        httpSendObject(joystick_data);
    };



    //=============================
    //======== return html ========
    //=============================
    return (
        <>

        <div style={{display:'flex', justifyContent:'center', alignItems:'center', height:'100vh'}}>
            <div>
                <div style={{position: 'absolute', top: '0'}}>
                    <h1>Joystick ctl</h1>
                </div>
                <Joystick 
                    size={joystickSize} 
                    sticky={false} 
                    baseColor="red" 
                    stickColor="blue" 
                    throttle={throttle}
                    move={handleMove} 
                    stop={handleStop}
                >
                </Joystick>
                <ul>
                    <li> x={x_html} </li>
                    <li> y={y_html} </li>
                    <li> radius={radius_html} </li>
                    <li> angle={angle_html} </li>
                </ul>
            </div>
        </div>

      {/*
      buttons for changing the api IP
      <div>
      <a>current ip used: {ip}</a>
      <button onClick={() => {setIp("10.0.0.66")}} >10.0.0.66 (BKA-network)</button>
      </div>
      */}
    
        </>
    );
}

export default App;




//del, testing, unused code
//---------------------------------------------
//--------- Send data via websocket -----------
//---------------------------------------------
//moved to normal POST request since websocket connection was unreliable on esp32
//    //create websocket
//    const websocket = useRef(null);
//    //const socketUrl = "ws://" + window.location.host + "/ws-api/servo";
//    const socketUrl = "ws://10.0.1.69/ws-api/joystick";
//    useEffect(() => {
//        websocket.current = new W3CWebSocket(socketUrl);
//        websocket.current.onmessage = (message) => {
//            console.log('got reply! ', message);
//        };
//        websocket.current.onopen = (event) => {
//            console.log('OPENED WEBSOCKET', event);
//            //sendJoystickData(0, 0, 0, 0);
//            websocket.current.send("");
//        };
//        websocket.current.onclose = (event) => {
//            console.log('CLOSED WEBSOCKET', event);
//        };
//        return () => websocket.current.close();
//    }, [])
//
//
//
//    //function for sending joystick data (provided as parameters) to controller via websocket
//    const sendJoystickDataWebsocket = (x, y, radius, angle) => {
//        //debug log
//        console.log("Sending:\n  X:" + x + "\n  Y:" + y + "\n  radius:" + radius + "\n  angle: " + angle);
//
//        websocket.current.send(
//            JSON.stringify({
//                x: x,
//                y: y,
//                radius: radius,
//                angle: angle
//            })
//        );
//    }

