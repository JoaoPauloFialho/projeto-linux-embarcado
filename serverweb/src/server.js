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

app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'interface', 'index.html'));
});

async function lerTemperatura() {
    try {
        const sensorPath = '/sys/bus/w1/devices/lembrar de substituir pelo id real do sensor da placa de xandão/w1_slave';
        const data = await fsPromises.readFile(sensorPath, 'utf8');
        
        const match = data.match(/t=(\d+)/);
        if (match) {
            return parseInt(match[1]) / 1000.0;
        }
    } catch (err) {
        return (Math.random() * 180) - 55; 
    }
    return null;
}

setInterval(async () => {
    const temp = await lerTemperatura();
    if (temp !== null) {
        const tempoAtual = new Date().toLocaleTimeString();
        io.emit('nova_temperatura', { tempo: tempoAtual, valor: temp });
    }
}, 1000);

server.listen(PORT, HOST, () => {
    console.log(`Servidor rodando em http://${HOST}:${PORT}`);
});

io.on('connection', (socket) => {
    console.log('Um computador conectou ao painel.');

    socket.on('atualizar_alarme', (novoValor) => {
        console.log(`Recebido novo limite de alarme: ${novoValor}°C`);
        
        const caminhoArquivoConf = path.join(__dirname, '..', '..', 'firmware', 'alarme.conf');
        
        fs.writeFile(caminhoArquivoConf, novoValor.toString(), (err) => {
            if (err) {
                console.error("Erro ao salvar o arquivo de configuração!", err);
            } else {
                console.log("Arquivo alarme.conf atualizado com sucesso!");
            }
        });
    });
});