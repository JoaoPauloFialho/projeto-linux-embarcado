#!/bin/bash

set -e

echo "  Instalação de Dependências  "

# verificando se node está instalado
if ! command -v node >/dev/null 2>&1; then
    echo "Node.js não encontrado. Instalando..."
    
    # verifica se o usuário tem privilégios de root para instalar pacotes
    if [ "$EUID" -ne 0 ]; then
        echo "Erro: Para instalar o Node.js, execute este script com sudo:"
        echo "sudo ./setup.sh"
        exit 1
    fi
    
    apt-get update
    apt-get install -y nodejs
    echo "Node.js instalado com sucesso: $(node -v)"
else
    echo "Node.js já está instalado: $(node -v)"
fi

# verifica se o npm está instalado
if ! command -v npm >/dev/null 2>&1; then
    echo "npm não encontrado. Tentando instalar..."
    
    if [ "$EUID" -ne 0 ]; then
        echo "Erro: Para instalar o npm, execute este script com sudo."
        exit 1
    fi
    
    apt-get install -y npm
    echo "npm instalado com sucesso: $(npm -v)"
else
    echo "npm já está instalado: $(npm -v)"
fi

# garantidor que o npm install rode na pasta certa
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"

echo "[*] Instalando as dependências do Node (npm install)..."
if [ "$EUID" -eq 0 ]; then
    sudo -u "$SUDO_USER" npm install
else
    npm install
fi