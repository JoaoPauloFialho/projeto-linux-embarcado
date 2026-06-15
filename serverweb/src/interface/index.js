const socket = io();

const LIMITE_PONTOS = 30;
const dadosTempo = [];
const dadosTemp = [];
const historicoCompleto = [];

const ctx = document.getElementById('meuGrafico').getContext('2d');
const graficoTemperatura = new Chart(ctx, {
    type: 'line',
    data: {
        labels: dadosTempo,
        datasets: [{
            label: 'Temperatura (°C)',
            data: dadosTemp,
            borderColor: '#ff6b6b',
            backgroundColor: 'rgba(255, 107, 107, 0.2)',
            borderWidth: 2,
            pointRadius: 2,
            tension: 0.4,
            fill: true
        }]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: true,
        color: '#e0e0e0', // Cor da legenda adaptada para o tema escuro
        scales: {
            x: {
                display: true,
                title: { display: true, text: 'Horário', color: '#e0e0e0' },
                ticks: { color: '#aaaaaa' },
                grid: { color: '#333333' } // Grade escura para não ofuscar a visão
            },
            y: {
                display: true,
                title: { display: true, text: 'Graus Celsius', color: '#e0e0e0' },
                ticks: { color: '#aaaaaa' },
                grid: { color: '#333333' },
                suggestedMin: 15,
                suggestedMax: 35
            }
        }
    }
});

// Elementos de alerta no HTML
const divAlerta = document.getElementById('alertaErro');
const spanMensagemErro = document.getElementById('mensagemErro');
const displayTemperatura = document.getElementById('valorTemperatura');

// Escuta o evento de falha crítica do servidor
socket.on('erro_sensor', (dados) => {
    spanMensagemErro.innerText = dados.mensagem;
    divAlerta.style.display = 'block';
    displayTemperatura.innerText = "OFF";
});

socket.on('nova_temperatura', (dados) => {
    // Oculta o alerta se o sensor for reconectado e voltar a funcionar
    divAlerta.style.display = 'none';

    const tempFormatada = parseFloat(dados.valor.toFixed(1));
    displayTemperatura.innerText = tempFormatada;

    historicoCompleto.push({ tempo: dados.tempo, valor: tempFormatada });

    dadosTempo.push(dados.tempo);
    dadosTemp.push(tempFormatada);

    //se passar do limite visual da tela, remove o elemento mais antigo
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

document.getElementById('btnBaixarCsv').addEventListener('click', () => {
    const linkDownload = document.createElement("a");
    linkDownload.href = "/download-csv";
    
    document.body.appendChild(linkDownload);
    linkDownload.click();
    document.body.removeChild(linkDownload);
});