const express = require('express');

const app = express();
app.use(express.json());

app.post('/sensor', (req, res) => {
    const { occupied } = req.body;

    console.log('Occupied:', occupied);

    res.sendStatus(200);
});

const PORT = 3000;
app.listen(PORT, () => {
    console.log(`Server running on http://localhost:${PORT}`);
});
