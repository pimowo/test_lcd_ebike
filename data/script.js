document.addEventListener('DOMContentLoaded', function() {
    // Aktualizacja czasu
    function updateTime() {
        const timeElement = document.querySelector('.time');
        const now = new Date();
        timeElement.textContent = now.toLocaleTimeString();
    }
    setInterval(updateTime, 1000);
    updateTime();

    // Obsługa WebSocket dla danych w czasie rzeczywistym
    const ws = new WebSocket(`ws://${window.location.hostname}/ws`);
    
    ws.onmessage = function(event) {
        const data = JSON.parse(event.data);
        updateDashboard(data);
    };

    function updateDashboard(data) {
        if(data.speed !== undefined) {
            document.querySelector('.speed .value').textContent = data.speed.toFixed(1);
        }
        if(data.temperature !== undefined) {
            document.querySelector('.temperature .value').textContent = data.temperature.toFixed(1);
        }
        if(data.battery !== undefined) {
            document.querySelector('.battery-status .value').textContent = data.battery;
            document.querySelector('.battery').textContent = `${data.battery}%`;
        }
        if(data.power !== undefined) {
            document.querySelector('.power .value').textContent = data.power;
        }
    }

    // Obsługa przycisków
    document.querySelectorAll('.btn-light').forEach(button => {
        button.addEventListener('click', function() {
            const light = this.dataset.light;
            fetch('/api/lights', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ light: light, state: !this.classList.contains('active') })
            })
            .then(response => response.json())
            .then(data => {
                if(data.status === 'ok') {
                    this.classList.toggle('active');
                }
            });
        });
    });

    document.querySelectorAll('.btn-assist').forEach(button => {
        button.addEventListener('click', function() {
            const direction = this.dataset.level;
            const levelElement = document.querySelector('.assist-level');
            let level = parseInt(levelElement.textContent);
            
            if(direction === 'up' && level < 5) {
                level++;
            } else if(direction === 'down' && level > 0) {
                level--;
            }

            fetch('/api/assist', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ level: level })
            })
            .then(response => response.json())
            .then(data => {
                if(data.status === 'ok') {
                    levelElement.textContent = level;
                }
            });
        });
    });
});
