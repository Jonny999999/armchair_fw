import React, { useCallback, useEffect, useState } from 'react';
import RestControl from './RestControl';
import { config } from '../config';
import { fetchRestStatus, sendRestAction, sendRestPercent } from '../api';


//====================================
//============ ChairView =============
//====================================
// adjust the leg- and back-rest positions
// note: kept on a separate tab so the buttons can not be hit while driving
export default function ChairView({ request }) {
    const [restStatus, setRestStatus] = useState(null);

    const handleAction = useCallback(
        (rest, action) => request(sendRestAction(rest, action)), [request]);
    const handlePercent = useCallback(
        (rest, percent) => request(sendRestPercent(rest, percent)), [request]);

    // repeatedly fetch the actual positions (they also change when using the encoder)
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
        <div className="chair-view">
            <RestControl
                label="Leg rest"
                status={restStatus ? restStatus.leg : null}
                onAction={(action) => handleAction('leg', action)}
                onPercent={(percent) => handlePercent('leg', percent)}
            />
            <RestControl
                label="Back rest"
                status={restStatus ? restStatus.back : null}
                onAction={(action) => handleAction('back', action)}
                onPercent={(percent) => handlePercent('back', percent)}
            />
        </div>
    );
}
