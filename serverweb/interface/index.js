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
    const tempFormatada = parseFloat(dados.valor.toFixed(1));
    document.getElementById('valorTemperatura').innerText = tempFormatada;

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
    
    if (historicoCompleto.length === 0) {
        alert("Aguarde. Nenhum dado de temperatura foi recebido ainda.");
        return;
    }

    let conteudoCSV = "HORARIO,TEMPERATURA_C\n";

    historicoCompleto.forEach(leitura => {
        conteudoCSV += `${leitura.tempo},${leitura.valor}\n`;
    });

    const blob = new Blob([conteudoCSV], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);

    const linkOculto = document.createElement("a");
    linkOculto.setAttribute("href", url);
    linkOculto.setAttribute("download", "Sessao_Monitoramento_BeagleBone.csv");
    
    document.body.appendChild(linkOculto);
    linkOculto.click();
    document.body.removeChild(linkOculto);
});