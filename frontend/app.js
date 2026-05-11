async function fetchData() {
    try {
        const res = await fetch('http://localhost:3000/sensor');
        if (!res.ok) throw new Error();
        const data = await res.json();

        document.getElementById('total').textContent = data.total;
        document.getElementById('occupied').textContent = data.occupied;
        document.getElementById('free').textContent = data.free;

        const lanesEl = document.getElementById('lanes');
        lanesEl.innerHTML = '';

        for (const [lane_id, slots] of Object.entries(data.lanes)) {
            const laneCard = document.createElement('div');
            laneCard.className = 'card lane-card';

            const title = document.createElement('h3');
            title.textContent = lane_id.replace('_', ' ');
            laneCard.appendChild(title);

            const grid = document.createElement('div');
            grid.className = 'slots-grid';

            for (const [slot_id, occupied] of Object.entries(slots)) {
                const slot = document.createElement('div');
                slot.className = 'slot ' + (occupied ? 'slot-occupied' : 'slot-free');
                slot.innerHTML = `<span>${slot_id.replace('_', ' ')}</span>
                                  <span class="slot-status">${occupied ? 'Occupied' : 'Free'}</span>`;
                grid.appendChild(slot);
            }

            laneCard.appendChild(grid);
            lanesEl.appendChild(laneCard);
        }

        document.getElementById('status').textContent = 'Backend reachable';
    } catch {
        document.getElementById('status').textContent = 'Backend not reachable';
    }
}

fetchData();
setInterval(fetchData, 500);
