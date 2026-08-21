import React, { useCallback, useEffect, useState } from 'react';
import DriveJoystick from './components/DriveJoystick';
import RestControl from './components/RestControl';
import { config } from './config';
import { fetchRestStatus, sendJoystick, sendRestAction, sendRestPercent } from './api';
import './App.css';


//====================================
//=============== App ================
//====================================
// remote control for the electric armchair, served by the esp32 itself
// (switch the armchair to HTTP mode, connect to wifi 'armchair')
export default function App() {
    const [isConnected, setIsConnected] = useState(true);
    const [restStatus, setRestStatus] = useState(null);

    // run an api call and track whether the chair is still reachable
    const request = useCallback(async (promise) => {
        try {
            const result = await promise;
            setIsConnected(true);
            return result;
        } catch (error) {
            console.warn(error);
            setIsConnected(false);
            return null;
        }
    }, []);

    //--- driving ---
    const handleJoystick = useCallback((x, y) => request(sendJoystick(x, y)), [request]);

    //--- chair adjustment ---
    const handleRestAction = useCallback(
        (rest, action) => request(sendRestAction(rest, action)), [request]);
    const handleRestPercent = useCallback(
        (rest, percent) => request(sendRestPercent(rest, percent)), [request]);

    // repeatedly fetch the actual rest positions (they also change when using the encoder)
    useEffect(() => {
        let isActive = true;
        const update = async () => {
            const status = await request(fetchRestStatus());
            if (isActive && status)
                setRestStatus(status);
        };
        update();
        const interval = setInterval(update, config.restPollIntervalMs);
        return () => { isActive = false; clearInterval(interval); };
    }, [request]);

    return (
        <div className="app">
            <header className="app-header">
                <h1>Armchair ctl</h1>
                <span className={`connection ${isConnected ? 'ok' : 'lost'}`}>
                    {isConnected ? 'connected' : 'no connection'}
                </span>
            </header>

            <main className="app-content">
                <DriveJoystick send={handleJoystick} />

                <div className="rest-controls">
                    <RestControl
                        label="Leg rest"
                        status={restStatus ? restStatus.leg : null}
                        onAction={(action) => handleRestAction('leg', action)}
                        onPercent={(percent) => handleRestPercent('leg', percent)}
                    />
                    <RestControl
                        label="Back rest"
                        status={restStatus ? restStatus.back : null}
                        onAction={(action) => handleRestAction('back', action)}
                        onPercent={(percent) => handleRestPercent('back', percent)}
                    />
                </div>
            </main>
        </div>
    );
}
