/**
 * @file ds18b20.h
 * @brief Interface para leitura do sensor DS18B20 via subsistema w1 (sysfs).
 * @author Eduardo Silva de Oliveira
 * @date 2026-06-12
 */

#ifndef DS18B20_H
#define DS18B20_H

/**
 * @brief Inicializa o barramento 1-Wire e localiza o sensor.
 * @return 0 em caso de sucesso, -1 se nenhum sensor for encontrado.
 */
int ds18b20_inicializar(void);

/**
 * @brief Realiza a leitura da temperatura atual em graus Celsius.
 * @return Valor da temperatura, ou -1000.0f em caso de erro ou falha de CRC.
 * @note Esta chamada bloqueia a execução por cerca de 750ms para a conversão do hardware.
 */
float ds18b20_ler_temperatura(void);

/**
 * @brief Libera os recursos e limpa o estado da biblioteca.
 */
void ds18b20_liberar_hardware(void);

#endif // DS18B20_H