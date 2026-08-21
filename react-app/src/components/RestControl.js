import React, { useEffect, useState } from 'react';
import { config } from '../config';


//====================================
//=========== RestControl ============
//====================================
// controls one of the two chair rests (leg / back):
//  - hold-buttons for moving up/down as long as pressed
//  - slider for moving to a certain position
//  - shows the position the controller currently tracks
export default function RestControl({ label, status, onAction, onPercent }) {
    const [target, setTarget] = useState(0);
    const [isDragging, setIsDragging] = useState(false);

    // follow the target of the controller while the user is not dragging the slider
    // (e.g. when the rest was moved with the encoder or the hold-buttons)
    useEffect(() => {
        if (!isDragging && status)
            setTarget(Math.round(status.target));
    }, [status, isDragging]);

    const position = status ? status.percent : 0;
    const isMoving = status ? status.state !== 'REST_OFF' : false;

    // move as long as the button is held
    // note: pointer capture makes sure the 'stop' still arrives when the finger slides off the button
    const holdButton = (action) => ({
        onPointerDown: (event) => {
            event.currentTarget.setPointerCapture(event.pointerId);
            onAction(action);
        },
        onPointerUp: () => onAction('stop'),
        onPointerCancel: () => onAction('stop'),
        onContextMenu: (event) => event.preventDefault(), // no popup on long press
    });

    return (
        <section className="panel rest-panel">
            <div className="rest-header">
                <span className="rest-label">{label}</span>
                <span className={`rest-position ${isMoving ? 'moving' : ''}`}>
                    {`${position.toFixed(0)}%`}
                </span>
            </div>

            <div className="rest-bar">
                <div className="rest-bar-fill" style={{ width: `${position}%` }} />
            </div>

            <div className="rest-buttons">
                <button className="hold-button" {...holdButton('down')}>&#9660; down</button>
                <button className="hold-button" {...holdButton('up')}>&#9650; up</button>
            </div>

            <div className="rest-slider">
                <input
                    type="range"
                    min="0"
                    max="100"
                    step={config.restSliderStep}
                    value={target}
                    onChange={(event) => { setIsDragging(true); setTarget(Number(event.target.value)); }}
                    // only send when the user is done dragging, otherwise the motor would
                    // start and stop for every intermediate value
                    onPointerUp={() => { setIsDragging(false); onPercent(target); }}
                    onKeyUp={() => { setIsDragging(false); onPercent(target); }}
                />
                <span className="rest-target">{`${target}%`}</span>
            </div>

            <div className="rest-presets">
                {/* note: re-sending 0/100 when already there is intentional, it re-runs into
                    the limit switch and thus re-syncs the tracked position */}
                <button onClick={() => onPercent(0)}>0%</button>
                <button onClick={() => onPercent(50)}>50%</button>
                <button onClick={() => onPercent(100)}>100%</button>
                <button onClick={() => onAction('stop')}>stop</button>
            </div>
        </section>
    );
}
