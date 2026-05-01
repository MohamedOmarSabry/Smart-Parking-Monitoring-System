const express = require('express');
const cors = require('cors');

const app = express();
app.use(cors());
app.use(express.json());

let state = {
    occupied: false,
};

app.post('/sensor', (req, res) => {
    const { occupied } = req.body;

    state.occupied = occupied;

    console.log('Occupied:', occupied);

    res.sendStatus(200);
});

app.get('/sensor', (req, res) => {
    res.json(state);
});

// Start server
const PORT = 3000;
app.listen(PORT, () => {
    console.log(`Server running on http://localhost:${PORT}`);
});
