#ifndef DS18B20_H
#define DS18B20_H

/**
 * @brief Inicializa os pinos GPIO para o sensor e para o MOSFET.
 * @return 0 em caso de sucesso, -1 em caso de falha.
 */
int ds18b20_inicializar_gpios(void);

/**
 * @brief Executa a Fase A: Solicita a conversão de temperatura e ativa o strong pull-up.
 */
void ds18b20_executar_fase_A(void);

/**
 * @brief Executa a Fase B: Lê a temperatura do Scratchpad.
 * @return A temperatura em graus Celsius, ou -1000.0 em caso de erro de comunicação.
 */
float ds18b20_executar_fase_B(void);

/**
 * @brief Libera os recursos de hardware alocados pela libgpiod.
 */
void ds18b20_liberar_hardware(void);

#endif /* DS18B20_H */