# Galton board no BitDogLab

Este projeto é uma simulação da Galton Board rodando no BitDogLab.

## O que faz?

Basicamente, você aperta um botão e bolinhas virtuais começam a cair no topo do display. Elas passam por uma série de "pinos" e, a cada "pino", decidem aleatoriamente se vão para a esquerda ou direita.

No final, as bolinhas se acumulam na base, formando  uma distribuição normal.

Tem também um botão para trocar de tela e ver quantas bolinhas já caíram no total.

## Como funciona por dentro:
* **Botões:**
    * Botão B para soltar uma nova bolinha na tábua.
    * Botão A para alternar entre a tela da simulação e uma tela que mostra o total de bolinhas que já caíram.
* **"Pinos" Virtuais Configuráveis:** No arquivo `display_oled.c`, você pode ajustar:
    * `PIN_START_X`: Onde o primeiro "andar" de pinos começa na horizontal.
    * `NUM_PIN_LEVELS`: Quantos "andares" de pinos você quer.
    * `PIN_X_INCREMENT`: Qual a distância horizontal entre os pinos.
    * `PROBABILITY_POSITIVE_Y_DEFLECTION_PERCENT`: A chance (em %) da bolinha desviar para baixo ao bater num pino. Por padrão, é 50%, dando chances iguais pra cima ou pra baixo.
* **Driver OLED Simplificado:** Foi criado um pequeno driver (`oled_driver.c` e `oled_driver.h`) para facilitar o desenho de pixels, linhas e texto no display, usando uma biblioteca base para o SSD1306 (`ssd1306_i2c.c` e companhia).

## Experimentos

* Mude as configurações dos "pinos" (`PIN_START_X`, `NUM_PIN_LEVELS`, `PIN_X_INCREMENT`) em `display_oled.c` para ver como a distribuição das bolinhas muda.
* Altere `PROBABILITY_POSITIVE_Y_DEFLECTION_PERCENT` para enviesar a queda das bolinhas.

É isso! Um projetinho legal pra brincar com gráficos, aleatoriedade e a Pico.
