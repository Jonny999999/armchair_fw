import React, { useEffect, useState } from 'react';
import { config } from '../config';
import { fetchStatus } from '../api';


//====================================
//============ StatusBar =============
//====================================
// Live battery/power readings, shown in the free space below the joystick.
// Note: deliberately not interactive (pointer-events are off in App.css) and of a fixed
// height - nothing here may move the joystick around or swallow a touch while driving.
export default function StatusBar({ request }) {
    const [status, setStatus] = useState(null);

    useEffect(() => {
        let isActive = true;
        const update = async () => {
            const result = await request(fetchStatus());
            if (isActive && result)
                setStatus(result);
        };
        update();
        const interval = setInterval(update, config.statusPollIntervalMs);
        return () => { isActive = false; clearInterval(interval); };
    }, [request]);

    // placeholders until the first response arrived, so the layout does not jump
    const value = (getter, unit, decimals = 0) =>
        status ? `${getter(status).toFixed(decimals)}${unit}` : `--${unit}`;

    return (
        <section className="panel status-panel">
            <div className="status-item">
                <span className="status-value">{value((s) => s.battery.percent, '%')}</span>
                <span className="status-label">{value((s) => s.battery.voltage, ' V', 1)}</span>
            </div>
            <div className="status-item">
                <span className="status-value">{value((s) => s.powerTotal, ' W')}</span>
                <span className="status-label">total</span>
            </div>
            <div className="status-item">
                <span className="status-value">{value((s) => s.motorLeft.power, ' W')}</span>
                <span className="status-label">left</span>
            </div>
            <div className="status-item">
                <span className="status-value">{value((s) => s.motorRight.power, ' W')}</span>
                <span className="status-label">right</span>
            </div>
        </section>
    );
}
