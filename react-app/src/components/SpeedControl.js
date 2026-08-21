import React, { useEffect, useState } from 'react';
import { config } from '../config';


//====================================
//=========== SpeedControl ===========
//====================================
// limits the top speed ('max duty', the same setting as in the encoder-menu)
// Placed at the top of the drive-view on purpose: it is changed often, but must not be
// hit accidentally while the thumb is on the joystick further down
export default function SpeedControl({ maxDuty, onChange }) {
    const [value, setValue] = useState(config.maxDutyMax);
    const [isDragging, setIsDragging] = useState(false);

    // follow the controller while the user is not dragging (can also be changed at the encoder)
    useEffect(() => {
        if (!isDragging && maxDuty !== null)
            setValue(maxDuty);
    }, [maxDuty, isDragging]);

    // only send when done dragging, the controller stores the value in nvs flash
    const commit = () => {
        setIsDragging(false);
        onChange(value);
    };

    return (
        <section className="panel speed-panel">
            <div className="speed-header">
                <span className="speed-label">Speed limit</span>
                <span className="speed-value">{`${value}%`}</span>
            </div>
            <input
                type="range"
                min={config.maxDutyMin}
                max={config.maxDutyMax}
                step={config.maxDutyStep}
                value={value}
                onChange={(event) => { setIsDragging(true); setValue(Number(event.target.value)); }}
                onPointerUp={commit}
                onKeyUp={commit}
            />
        </section>
    );
}
