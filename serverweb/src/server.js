const fsPromises = require('fs').promises;
const express = require('express');
const http = require('http');
const { Server } = require("socket.io");
const fs = require('fs');
const path = require('path');

const app = express();
app.use(express.static(path.join(__dirname, 'interface')));

const server = http.createServer(app);
const io = new Server(server);

const PORT = 8080;
const HOST = '0.0.0.0';

const caminhoArquivoConf = path.join(__dirname, '..', '..', 'firmware', 'alarme.conf');

try {
    // Valor default do alarme  
    fs.writeFileSync(caminhoArquivoConf, '30.0');
    console.log("Inicialização: alarme.conf configurado para 30.0°C");
} catch (err) {
    console.error("Erro fatal ao inicializar o arquivo alarme.conf:", err);
}

app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'interface', 'index.html'));
});

async function lerTemperatura() {
    try {
        const sensorPath = '/sys/bus/w1/devices/28-000000b9f662/w1_slave';
        const data = await fsPromises.readFile(sensorPath, 'utf8');
        
        const match = data.match(/t=(\d+)/);
        if (match) {
            return parseInt(match[1]) / 1000.0;
        }
    } catch (err) {
        return null; 
    }
    return null;
}

setInterval(async () => {
    const temp = await lerTemperatura();
    const tempoAtual = new Date().toLocaleTimeString();

    if (temp !== null) {
        // Hardware OK: Envia a temperatura normal
        io.emit('nova_temperatura', { tempo: tempoAtual, valor: temp });
    } else {
        // Hardware Falhou: Dispara o alerta crítico para a rede!
        io.emit('erro_sensor', { 
            mensagem: "ALERTA CRÍTICO: Sensor de Temperatura Desconectado!" 
        });
    }
}, 1000);

server.listen(PORT, HOST, () => {
    console.log(`Servidor rodando em http://${HOST}:${PORT}`);
});

io.on('connection', (socket) => {
    console.log('Um computador conectou ao painel.');

    socket.on('atualizar_alarme', (novoValor) => {
        console.log(`Recebido novo limite de alarme: ${novoValor}°C`);
        
        fs.writeFile(caminhoArquivoConf, novoValor.toString(), (err) => {
            if (err) {
                console.error("Erro ao salvar o arquivo de configuração!", err);
            } else {
                console.log("Arquivo alarme.conf atualizado com sucesso!");
            }
        });
    });
});