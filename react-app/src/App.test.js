import { render, screen, act, fireEvent, waitFor } from '@testing-library/react';
import App from './App';

// mocked answer of GET /api/chair
const restStatus = {
    leg: { percent: 42.4, target: 100, state: 'REST_UP' },
    back: { percent: 10, target: 10, state: 'REST_OFF' },
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
        return Promise.resolve({ ok: true, json: () => Promise.resolve(restStatus) });
    });
});

const postedTo = (path) =>
    requests.filter((request) => request.method === 'POST' && request.url.endsWith(path))
        .map((request) => JSON.parse(request.body));


test('shows the rest positions received from the controller', async () => {
    render(<App />);
    await waitFor(() => expect(screen.getByText('42%')).toBeInTheDocument());
    // position of both rests is shown (back-rest '10%' also appears as slider value)
    expect(screen.getAllByText('10%').length).toBeGreaterThan(0);
    expect(screen.getByText('connected')).toBeInTheDocument();
});


test('repeats the last joystick position as heartbeat', async () => {
    jest.useFakeTimers();
    render(<App />);
    await act(async () => {}); // let the initial status request settle
    requests = [];

    act(() => { jest.advanceTimersByTime(1000); });

    expect(postedTo('/api/joystick')).toEqual([{ x: 0, y: 0 }]);
    jest.useRealTimers();
});


test('hold-button moves the rest while pressed and stops on release', async () => {
    render(<App />);
    await waitFor(() => expect(screen.getByText('42%')).toBeInTheDocument());
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
    await waitFor(() => expect(screen.getByText('42%')).toBeInTheDocument());
    requests = [];

    fireEvent.click(screen.getAllByText('100%')[1]); // [0] is the slider value of the leg rest

    expect(postedTo('/api/chair')).toEqual([{ rest: 'leg', percent: 100 }]);
});
