const express = require('express');
const cors = require('cors');

const app = express();
app.use(cors());
app.use(express.json());

// State: { lanes: { "LANE_1": { slots: { "SLOT_1": { occupied: false } } } } }
let state = { lanes: {} };

// POST /sensor — receives full parking state from main ESP32
// Body: { lanes: [ { lane_id, slots: [ { slot_id, occupied } ] } ] }
app.post('/sensor', (req, res) => {
    const { lanes } = req.body;
    if (!Array.isArray(lanes)) return res.status(400).json({ error: 'Expected { lanes: [...] }' });

    for (const lane of lanes) {
        const { lane_id, slots } = lane;
        if (!lane_id || !Array.isArray(slots)) continue;
        if (!state.lanes[lane_id]) state.lanes[lane_id] = { slots: {} };
        for (const slot of slots) {
            const { slot_id, occupied } = slot;
            if (!slot_id || typeof occupied !== 'boolean') continue;
            state.lanes[lane_id].slots[slot_id] = { occupied };
        }
    }

    console.log('State updated:', JSON.stringify(state, null, 2));
    res.sendStatus(200);
});

// GET /sensor — returns summary + per-lane/slot detail
app.get('/sensor', (req, res) => {
    let total = 0,
        occupied = 0;
    const lanesOut = {};

    for (const [lane_id, lane] of Object.entries(state.lanes)) {
        lanesOut[lane_id] = {};
        for (const [slot_id, slot] of Object.entries(lane.slots)) {
            total++;
            if (slot.occupied) occupied++;
            lanesOut[lane_id][slot_id] = slot.occupied;
        }
    }

    res.json({ total, occupied, free: total - occupied, lanes: lanesOut });
});

app.listen(3000, () => console.log('Backend running on http://localhost:3000'));
