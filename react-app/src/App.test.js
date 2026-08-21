import { render, screen, act, fireEvent, waitFor } from '@testing-library/react';
import App from './App';

// mocked answers of the esp32
const restStatus = {
    leg: { percent: 42.4, target: 100, state: 'REST_UP' },
    back: { percent: 10, target: 10, state: 'REST_OFF' },
};
const settings = { maxDuty: 65 };

let requests = [];

beforeEach(() => {
    requests = [];
    global.fetch = jest.fn((url, options) => {
        requests.push({
            url,
            method: (options && options.method) || 'GET',
            body: options && options.body,
        });
        return Promise.resolve({
            ok: true,
            json: () => Promise.resolve(url.endsWith('/api/settings') ? settings : restStatus),
        });
    });
});

const postedTo = (path) =>
    requests.filter((request) => request.method === 'POST' && request.url.endsWith(path))
        .map((request) => JSON.parse(request.body));

// the chair controls are on the second tab
const openChairTab = async () => {
    fireEvent.click(screen.getByText('Chair'));
    await waitFor(() => expect(screen.getByText('42%')).toBeInTheDocument());
};


//========== drive view ==========

test('shows the speed limit configured on the controller', async () => {
    render(<App />);
    await waitFor(() => expect(screen.getByText('65%')).toBeInTheDocument());
    expect(screen.getByText('connected')).toBeInTheDocument();
});


test('repeats the last joystick position as heartbeat', async () => {
    jest.useFakeTimers();
    render(<App />);
    await act(async () => {}); // let the initial requests settle
    requests = [];

    act(() => { jest.advanceTimersByTime(1000); });

    expect(postedTo('/api/joystick')).toEqual([{ x: 0, y: 0 }]);
    jest.useRealTimers();
});


test('speed slider sends the new max duty when released', async () => {
    render(<App />);
    await waitFor(() => expect(screen.getByText('65%')).toBeInTheDocument());
    requests = [];

    const slider = screen.getByRole('slider');
    fireEvent.change(slider, { target: { value: '40' } });
    expect(postedTo('/api/settings')).toEqual([]); // not while dragging (nvs write)

    fireEvent.pointerUp(slider);
    expect(postedTo('/api/settings')).toEqual([{ maxDuty: 40 }]);
});


//========== chair view ==========

test('switching to the chair tab stops the chair and shows the rest positions', async () => {
    render(<App />);
    await waitFor(() => expect(screen.getByText('65%')).toBeInTheDocument());
    requests = [];

    await openChairTab();

    // the joystick is unmounted -> a final 'center' is sent instead of waiting for the timeout
    expect(postedTo('/api/joystick')).toEqual([{ x: 0, y: 0 }]);
    expect(screen.getAllByText('10%').length).toBeGreaterThan(0);
});


test('hold-button moves the rest while pressed and stops on release', async () => {
    render(<App />);
    await openChairTab();
    requests = [];

    const upButton = screen.getAllByText(/up/)[0];
    upButton.setPointerCapture = () => {}; // not implemented by jsdom
    fireEvent.pointerDown(upButton, { pointerId: 1 });
    fireEvent.pointerUp(upButton, { pointerId: 1 });

    expect(postedTo('/api/chair')).toEqual([
        { rest: 'leg', action: 'up' },
        { rest: 'leg', action: 'stop' },
    ]);
});


test('preset button sends the target position', async () => {
    render(<App />);
    await openChairTab();
    requests = [];

    fireEvent.click(screen.getAllByText('100%')[1]); // [0] is the slider value of the leg rest

    expect(postedTo('/api/chair')).toEqual([{ rest: 'leg', percent: 100 }]);
});
