import { render, screen, act, fireEvent, waitFor } from '@testing-library/react';
import App from './App';

// mocked answers of the esp32
const restStatus = {
    leg: { percent: 42.4, target: 100, state: 'REST_UP' },
    back: { percent: 10, target: 10, state: 'REST_OFF' },
};
const settings = { maxDuty: 65 };
const status = {
    battery: { percent: 82.4, voltage: 27.35 },
    motorLeft: { current: 3.2, power: 88, duty: 45 },
    motorRight: { current: 2.9, power: 79, duty: 45 },
    powerTotal: 167,
};

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
            json: () => {
                if (url.endsWith('/api/settings')) return Promise.resolve(settings);
                if (url.endsWith('/api/status')) return Promise.resolve(status);
                return Promise.resolve(restStatus);
            },
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


test('a cancelled joystick gesture centers the chair instead of driving on', async () => {
    jest.useFakeTimers();
    render(<App />);
    await act(async () => {});

    // drag the stick, then let the browser cancel the gesture instead of emitting pointerup
    const stick = document.querySelector('.joystick-panel button');
    stick.setPointerCapture = () => {}; // not implemented by jsdom
    fireEvent.pointerDown(stick, { pointerId: 1, clientX: 0, clientY: 0 });
    fireEvent.pointerMove(window, { pointerId: 1, clientX: 0, clientY: -100 });
    requests = [];
    fireEvent.pointerCancel(window, { pointerId: 1 });

    expect(postedTo('/api/joystick')).toEqual([{ x: 0, y: 0 }]);

    // and the heartbeat repeats the centered position, not the last driving one
    requests = [];
    act(() => { jest.advanceTimersByTime(1000); });
    expect(postedTo('/api/joystick')).toEqual([{ x: 0, y: 0 }]);
    jest.useRealTimers();
});


test('the pull-to-refresh gesture is cancelled, except on sliders and the chair-view', async () => {
    render(<App />);
    await act(async () => {});

    // dragging anywhere in the drive view (joystick, empty space, header) must not reach the browser
    const onJoystick = new TouchEvent('touchmove', { bubbles: true, cancelable: true });
    document.querySelector('.joystick-panel button').dispatchEvent(onJoystick);
    expect(onJoystick.defaultPrevented).toBe(true);

    // the speed slider still has to be draggable
    const onSlider = new TouchEvent('touchmove', { bubbles: true, cancelable: true });
    screen.getByRole('slider').dispatchEvent(onSlider);
    expect(onSlider.defaultPrevented).toBe(false);

    // ... and the chair view has to stay scrollable
    await openChairTab();
    const onChairView = new TouchEvent('touchmove', { bubbles: true, cancelable: true });
    document.querySelector('.chair-view').dispatchEvent(onChairView);
    expect(onChairView.defaultPrevented).toBe(false);
});


test('shows the live battery and motor stats below the joystick', async () => {
    render(<App />);
    await waitFor(() => expect(screen.getByText('82%')).toBeInTheDocument());

    expect(screen.getByText('27.4 V')).toBeInTheDocument();
    expect(screen.getByText('167 W')).toBeInTheDocument(); // total
    expect(screen.getByText('88 W')).toBeInTheDocument();  // left
    expect(screen.getByText('79 W')).toBeInTheDocument();  // right
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


test('the back-rest buttons are labelled by the physical movement (0% is upright)', async () => {
    render(<App />);
    await openChairTab();
    requests = [];

    // 'flatten' has to send 'up' and 'upright' has to send 'down' - the other way round
    // than for the leg rest, see ChairView
    const flatten = screen.getByText(/flatten/);
    const upright = screen.getByText(/upright/);
    [flatten, upright].forEach((button) => { button.setPointerCapture = () => {}; });

    fireEvent.pointerDown(flatten, { pointerId: 1 });
    fireEvent.pointerUp(flatten, { pointerId: 1 });
    fireEvent.pointerDown(upright, { pointerId: 1 });
    fireEvent.pointerUp(upright, { pointerId: 1 });

    expect(postedTo('/api/chair')).toEqual([
        { rest: 'back', action: 'up' },
        { rest: 'back', action: 'stop' },
        { rest: 'back', action: 'down' },
        { rest: 'back', action: 'stop' },
    ]);
});


test('preset button sends the target position', async () => {
    render(<App />);
    await openChairTab();
    requests = [];

    fireEvent.click(screen.getAllByText('100%')[1]); // [0] is the slider value of the leg rest

    expect(postedTo('/api/chair')).toEqual([{ rest: 'leg', percent: 100 }]);
});


test('pressing the same preset again re-sends it (re-syncs the position at the limit switch)', async () => {
    render(<App />);
    await openChairTab();
    requests = [];

    const preset0 = screen.getAllByText('0%')[0]; // leg rest
    fireEvent.click(preset0);
    fireEvent.click(preset0);

    expect(postedTo('/api/chair')).toEqual([
        { rest: 'leg', percent: 0 },
        { rest: 'leg', percent: 0 },
    ]);
});


test('releasing the rest slider sends even when the value was not changed', async () => {
    render(<App />);
    await openChairTab();
    requests = [];

    // leg rest slider, already at its current target (100%) -> no 'change' event at all
    const slider = screen.getAllByRole('slider')[0];
    fireEvent.pointerUp(slider);

    expect(postedTo('/api/chair')).toEqual([{ rest: 'leg', percent: 100 }]);
});


test('dragging the rest slider only sends once, on release', async () => {
    render(<App />);
    await openChairTab();
    requests = [];

    const slider = screen.getAllByRole('slider')[0];
    fireEvent.change(slider, { target: { value: '40' } });
    fireEvent.change(slider, { target: { value: '60' } });
    expect(postedTo('/api/chair')).toEqual([]); // nothing while dragging

    fireEvent.pointerUp(slider);
    expect(postedTo('/api/chair')).toEqual([{ rest: 'leg', percent: 60 }]);
});
