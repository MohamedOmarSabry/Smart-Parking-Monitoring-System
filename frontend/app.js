const TOTAL_SLOTS = 1;

async function fetchData() {
    try {
        const res = await fetch('http://localhost:3000/sensor');

        if (!res.ok) {
            throw new Error('Backend error');
        }

        const data = await res.json();

        const occupied = data.occupied ? 1 : 0;
        const free = TOTAL_SLOTS - occupied;

        document.getElementById('occupied').textContent = occupied;
        document.getElementById('free').textContent = free;

        document.getElementById('status').innerText = 'Backend reachable';
    } catch (err) {
        document.getElementById('status').innerText = 'Backend not reachable';
    }
}

fetchData();

// Poll every second
setInterval(fetchData, 500);
