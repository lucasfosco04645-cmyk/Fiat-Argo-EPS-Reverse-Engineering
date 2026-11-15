# Fiat Argo EPS — Reverse Engineering

**Projeto:** Pesquisa e engenharia reversa experimental da direção elétrica Nexteer CEPS do Fiat Argo/Cronos  
**Objetivo:** Investigar a comunicação CAN e o diagnóstico da coluna em bancada, visando compreender os requisitos eletrônicos para sua adaptação.

> **Status:** Comunicação CAN observada e comunicação diagnóstica UDS obtida em teste. Desbloqueio da assistência elétrica ainda não confirmado.

## 1. Identificação da coluna

| Identificação | Informação |
|---|---|
| Família | Nexteer CEPS |
| FIAT GIS | 01002692210 |
| Supplier | 020748 |
| Número de série | 01087 |
| Data da etiqueta | 30/08/2021 |
| Motor | 38023703 / BLM195 |
| Software identificado | X6BL_2.6.0 |

## 2. Pinagem estabelecida na bancada

Conector pequeno de seis posições, com cinco terminais ocupados. A ligação estabelecida nos testes é:

| Pinos | Cor observada | Função / ligação |
|---|---|---|
| **1** | **Laranja/branco** | **Positivo de ativação que arma o relé** |
| **2 + 4** | **Marrom + marrom** | **CAN-L — unir os dois fios marrons** |
| **3 + 5** | **Verde + verde** | **CAN-H — unir os dois fios verdes** |
| 6 | Vazio | Sem terminal instalado |

**Importante:** conforme a pinagem estabelecida nos testes, os dois fios marrons (pinos 2 e 4) ficam unidos como CAN-L, e os dois fios verdes (pinos 3 e 5) ficam unidos como CAN-H. O pino 1 é o positivo de ativação que arma o relé; não confundir com a alimentação principal de potência da direção.

A pinagem é registro experimental desta unidade. Antes de energizar, conferir continuidade, polaridade, isolamento e esquema elétrico aplicável. Não usar esta tabela como substituta de verificação elétrica no componente específico.

## 3. Comunicação CAN confirmada

Hardware utilizado:

- Arduino Uno
- Módulo MCP2515 HW-184
- Cristal de 8 MHz
- CAN em 500 kbps

Mensagens observadas:

| ID | Resultado |
|---|---|
| 0x0DE | Quadro periódico da EPS, aproximadamente 100 Hz |
| 0x002 | Quadro observado ocasionalmente, aproximadamente 1 Hz, dado `04` |

Exemplo de sequência observada no ID `0x0DE`:

```text
1C 20 B7 D0 80 A5
1C 20 B7 D0 81 B8
1C 20 B7 D0 82 9F
1C 20 B7 D0 83 82
1C 20 B7 D0 84 D1
1C 20 B7 D0 85 CC
1C 20 B7 D0 86 EB
1C 20 B7 D0 87 F6
1C 20 B7 D0 88 4D
1C 20 B7 D0 89 50
1C 20 B7 D0 8A 77
```

Os quatro primeiros bytes permaneceram constantes nos registros apresentados. O quinto byte percorreu uma sequência de valores, sugerindo contador ou estado. O algoritmo do último byte ainda não foi identificado.

## 4. Avanço mais recente — diagnóstico UDS

Foi desenvolvido um teste com:

- Comunicação CAN estendida;
- Transporte ISO-TP, incluindo Single Frame, First Frame e Consecutive Frame;
- Envio de Flow Control;
- Solicitação de sessão estendida;
- Solicitação de limpeza de DTCs;
- Consulta de quantidade e lista detalhada de DTCs.

IDs configurados no último sketch:

| Direção | ID |
|---|---|
| Transmissão UDS | `0x18DA30F1` |
| Recepção UDS | `0x18DAF130` |

Resposta registrada:

```text
[UDS] Consultando quantidade de DTCs (19 01)...
RAW RX [0x18DAF130]: 06 59 01 FF 00 00 13
```

A resposta positiva `59 01` contém o valor `0x0013`, interpretado pelo sketch como **19 em decimal**.

Esse resultado representa um avanço importante: houve resposta diagnóstica da coluna no endereço configurado pelo teste. A quantidade indicada não deve ser confundida com 19 códigos individuais já identificados. A confirmação e interpretação dos registros detalhados dependem da análise integral da resposta `19 02`.

## 5. Código do último experimento

O sketch original está incluído integralmente em:

`firmware/argo_eps_last_experiment.ino`

É o código utilizado no teste que recebeu a resposta `59 01` com o valor interpretado como 19. Ele contém a emulação de tráfego CAN, a máquina de estados UDS e o tratamento ISO-TP.

O arquivo foi preservado sem simplificação ou alteração do conteúdo original. Os quadros simulados são hipóteses experimentais e não devem ser tratados como mensagens oficiais ou validadas de um Fiat Cronos/Argo.

## 6. Linha do tempo do progresso

1. Identificação física da coluna Nexteer CEPS.
2. Estabelecimento da pinagem de ativação e dos pares CAN.
3. Captura de mensagens CAN em 500 kbps.
4. Análise da mensagem periódica `0x0DE`.
5. Implementação de comunicação UDS e transporte ISO-TP.
6. Recebimento de resposta positiva `59 01`, com valor interpretado como 19.
7. Próxima etapa: validar os DTCs detalhados e identificar as mensagens reais necessárias para habilitar a assistência.

## 7. O que ainda não foi confirmado

- Sequência CAN necessária para habilitar a assistência.
- Mensagens reais de velocidade e estado do motor.
- Algoritmo do último byte do quadro `0x0DE`.
- Significado e origem de cada DTC retornado.
- Se a coluna pode operar de forma independente do Body Computer original.

## 8. Segurança

Este projeto é experimental e envolve um sistema de direção, componente crítico de segurança veicular.

Os testes devem ser realizados em bancada controlada, com alimentação protegida por fusível, limitação de corrente e desligamento de emergência. Não utilizar quadros CAN não validados em um veículo em circulação. O código inclui transmissão de mensagens simuladas e solicitação de limpeza de DTCs.

---

**Projeto em desenvolvimento.** Os resultados documentados distinguem observações realizadas de hipóteses ainda em investigação.
