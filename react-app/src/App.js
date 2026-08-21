import React, { useCallback, useEffect, useState } from 'react';
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

    //--- disable the pull-to-refresh gesture ---
    // Dragging the joystick downwards triggered the reload-gesture of the phone browser,
    // which made driving backwards impossible. Doing this via css (overscroll-behavior /
    // touch-action, both still set in App.css) turned out not to be reliable, cancelling the
    // touchmove itself is. Note: has to be registered non-passive, touchmove listeners are
    // passive by default and preventDefault() is ignored on those.
    // Note: this does NOT break the joystick - the browser only sends 'pointercancel' when it
    // takes the pointer over for scrolling, which is exactly what is prevented here.
    useEffect(() => {
        const preventPullToRefresh = (event) => {
            // sliders must stay draggable and the chair-view scrollable
            const target = event.target instanceof Element ? event.target : null;
            if (target && target.closest('input[type="range"], .chair-view'))
                return;
            if (event.cancelable)
                event.preventDefault();
        };
        document.addEventListener('touchmove', preventPullToRefresh, { passive: false });
        return () => document.removeEventListener('touchmove', preventPullToRefresh);
    }, []);

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
                {/* Escape hatch out of the captive-portal browser of the phone (see
                    handleCaptivePortalProbe in http.cpp): that webview handles touch
                    gestures differently and is not meant to stay open. 'target=_blank'
                    is what makes most webviews hand the url to the actual browser. */}
                <a className="open-in-browser" href="http://armchair.local/"
                   target="_blank" rel="noreferrer">open in browser &#8599;</a>
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
