import React, { useCallback, useEffect, useState } from 'react';
import DriveJoystick from './DriveJoystick';
import SpeedControl from './SpeedControl';
import { config } from '../config';
import { fetchSettings, sendJoystick, sendMaxDuty } from '../api';


//====================================
//============ DriveView =============
//====================================
// everything needed while driving: speed limit on top, joystick at the bottom
// where the thumb rests - nothing else is tappable near the joystick
export default function DriveView({ request }) {
    const [maxDuty, setMaxDuty] = useState(null);

    const handleJoystick = useCallback((x, y) => request(sendJoystick(x, y)), [request]);

    const handleMaxDuty = useCallback(async (value) => {
        setMaxDuty(value); // show immediately, gets confirmed by the next poll
        await request(sendMaxDuty(value));
    }, [request]);

    // fetch the currently configured max duty (can also be changed at the encoder-menu)
    useEffect(() => {
        let isActive = true;
        const update = async () => {
            const settings = await request(fetchSettings());
            if (isActive && settings)
                setMaxDuty(settings.maxDuty);
        };
        update();
        const interval = setInterval(update, config.settingsPollIntervalMs);
        return () => { isActive = false; clearInterval(interval); };
    }, [request]);

    return (
        <div className="drive-view">
            <SpeedControl maxDuty={maxDuty} onChange={handleMaxDuty} />
            <DriveJoystick send={handleJoystick} />
        </div>
    );
}
