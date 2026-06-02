const socket = io();

const LIMITE_PONTOS = 30;
const dadosTempo = [];
const dadosTemp = [];

const ctx = document.getElementById('meuGrafico').getContext('2d');
const graficoTemperatura = new Chart(ctx, {
    type: 'line',
    data: {
        labels: dadosTempo,
        datasets: [{
            label: 'Temperatura (°C)',
            data: dadosTemp,
            borderColor: '#e74c3c',
            backgroundColor: 'rgba(231, 76, 60, 0.2)',
            borderWidth: 2,
            pointRadius: 2,
            tension: 0.4,
            fill: true
        }]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: false,
        scales: {
            x: {
                display: true,
                title: { display: true, text: 'Horário' }
            },
            y: {
                display: true,
                title: { display: true, text: 'Graus Celsius' },
                suggestedMin: 15,
                suggestedMax: 35
            }
        }
    }
});

socket.on('nova_temperatura', (dados) => {
    document.getElementById('valorTemperatura').innerText = dados.valor.toFixed(2);

    dadosTempo.push(dados.tempo);
    dadosTemp.push(dados.valor);

    // se passar do limite, remove o elemento mais antigo
    if (dadosTempo.length > LIMITE_PONTOS) {
        dadosTempo.shift();
        dadosTemp.shift();
    }

    graficoTemperatura.update();
});

const sliderAlarme = document.getElementById('sliderAlarme');
const valorAlarmeDisplay = document.getElementById('valorAlarme');

sliderAlarme.addEventListener('input', (evento) => {
    valorAlarmeDisplay.innerText = parseFloat(evento.target.value).toFixed(1);
});

sliderAlarme.addEventListener('change', (evento) => {
    const novoLimite = evento.target.value;
    socket.emit('atualizar_alarme', novoLimite);
});