import React, { useCallback, useEffect, useRef, useState } from 'react';
import { Joystick } from 'react-joystick-component';
import { config } from '../config';


//------------------------------------
//--------- scaleCoordinate ----------
//------------------------------------
// scale a coordinate from the joystick element (pixels) to a value of -1 to 1
// note: the tolerance around the axis ('snap to zero') is applied by the controller
const scaleCoordinate = (input) =>
    Number((input / (config.joystickSize / 2)).toFixed(config.decimalPlaces));


//====================================
//========== DriveJoystick ===========
//====================================
// virtual joystick for driving the armchair (HTTP control-mode)
export default function DriveJoystick({ send }) {
    const [coordinates, setCoordinates] = useState({ x: 0, y: 0 });
    // last sent position, repeatedly re-sent as heartbeat (see below)
    const lastSent = useRef({ x: 0, y: 0 });

    const sendCoordinates = useCallback((x, y) => {
        lastSent.current = { x, y };
        setCoordinates({ x, y });
        send(x, y);
    }, [send]);

    //--- heartbeat ---
    // repeat the last position in a fixed interval, otherwise the controller runs into
    // its 'no data received' timeout and stops the motors while the stick is just being
    // held still (e.g. driving straight ahead for a while). See config.js
    useEffect(() => {
        const interval = setInterval(() => {
            const { x, y } = lastSent.current;
            send(x, y);
        }, config.heartbeatIntervalMs);
        return () => {
            clearInterval(interval);
            // leaving the drive-view stops the heartbeat -> stop the chair right away
            // instead of letting it run into the timeout
            send(0, 0);
        };
    }, [send]);

    //--- joystick events ---
    const handleMove = (event) =>
        sendCoordinates(scaleCoordinate(event.x), scaleCoordinate(event.y));

    const handleStop = () => sendCoordinates(0, 0);

    return (
        <section className="panel joystick-panel">
            <Joystick
                size={config.joystickSize}
                sticky={false}
                baseColor="#8d0801"
                stickColor="#708d81"
                throttle={config.joystickThrottleMs}
                move={handleMove}
                stop={handleStop}
            />
            <div className="coordinates">
                x={coordinates.x.toFixed(2)} &nbsp; y={coordinates.y.toFixed(2)}
            </div>
        </section>
    );
}
