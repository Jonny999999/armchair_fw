import React, { useCallback, useState } from 'react';
import DriveView from './components/DriveView';
import ChairView from './components/ChairView';
import './App.css';

const TABS = [
    { id: 'drive', label: 'Drive' },
    { id: 'chair', label: 'Chair' },
];


//====================================
//=============== App ================
//====================================
// remote control for the electric armchair, served by the esp32 itself
// (switch the armchair to HTTP mode, connect to wifi 'armchair')
export default function App() {
    const [isConnected, setIsConnected] = useState(true);
    const [activeTab, setActiveTab] = useState('drive');

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

    return (
        <div className="app">
            <header className="app-header">
                <h1>Armchair ctl</h1>
                <span className={`connection ${isConnected ? 'ok' : 'lost'}`}>
                    {isConnected ? 'connected' : 'no connection'}
                </span>
            </header>

            <nav className="tabs">
                {TABS.map((tab) => (
                    <button
                        key={tab.id}
                        className={`tab ${activeTab === tab.id ? 'active' : ''}`}
                        onClick={() => setActiveTab(tab.id)}
                    >
                        {tab.label}
                    </button>
                ))}
            </nav>

            <main className="app-content">
                {/* note: rendered conditionally on purpose - leaving the drive-view unmounts
                    the joystick, which stops the heartbeat and sends a final 'center' */}
                {activeTab === 'drive' ? <DriveView request={request} /> : <ChairView request={request} />}
            </main>
        </div>
    );
}
