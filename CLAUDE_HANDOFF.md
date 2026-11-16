# Pacote de transferência para Claude — Engenharia reversa da EPS Fiat Argo

Este documento reúne o estado conhecido da investigação e um pedido de trabalho para um assistente de programação. O código original está em `firmware/argo_eps_last_experiment.ino`; o resumo público está no `README.md`; a solicitação de revisão técnica também está na Issue #1.

## Objetivo do projeto

Investigar em bancada uma coluna de direção elétrica Nexteer CEPS de aplicação Fiat Argo/Cronos, com intenção de compreender sua comunicação CAN e diagnóstico e avaliar os requisitos para futura adaptação ao Fiat Uno 1990.

**Estado real:** foram observados quadros CAN periódicos e obtida uma resposta diagnóstica UDS. Ainda não foi comprovado o desbloqueio nem o funcionamento da assistência elétrica independente do Body Computer. Não afirmar o contrário.

## Identificação da unidade

- Família: Nexteer CEPS
- FIAT GIS: 01002692210
- Supplier: 020748
- Número de série: 01087
- Data na etiqueta: 30/08/2021
- Motor: 38023703 / BLM195
- Software reportado pela leitura DID F180: `X6BL_2.6.0`

## Pinagem estabelecida pelo usuário em teste

Conector pequeno de seis posições, com cinco terminais ocupados:

| Pino | Fio | Ligação estabelecida |
|---|---|---|
| 1 | Laranja/branco | Positivo de ativação que arma o relé; não é a alimentação principal de potência |
| 2 | Marrom | Unir ao pino 4; CAN-L |
| 3 | Verde | Unir ao pino 5; CAN-H |
| 4 | Marrom | Unir ao pino 2; CAN-L |
| 5 | Verde | Unir ao pino 3; CAN-H |
| 6 | Sem fio | Vazio |

Esta é a correção mais recente dada pelo usuário. Notas antigas que atribuíam outras funções aos pinos 2/3/4/5 ou colocavam ativação no pino 6 foram superadas por essa confirmação. A alimentação principal de potência e os aterramentos de potência são circuitos distintos e não devem ser inferidos desta tabela. Confirmar eletricamente antes de energizar.

Medições históricas: pontas do multímetro juntas ~0,09 Ω; entre 2–4 ~0,9 Ω; entre 3–5 ~0,9 Ω. Essas medições por si só não estabelecem pinagem, foram combinadas com os testes de comunicação descritos pelo usuário.

## Bancada e CAN

- Arduino Uno
- MCP2515 HW-184
- Cristal MCP2515 de 8 MHz
- CAN 500 kbit/s

Observações recebidas:
- ID padrão `0x0DE`, DLC 6, frequência aproximada de 100 Hz.
- Outro ID padrão `0x002`, aproximadamente 1 Hz, payload observado `04`.
- Exemplo de payload do `0x0DE`: `1C 20 B7 D0 82 9F`.
- Nos registros, os quatro primeiros bytes permaneceram em `1C 20 B7 D0`; o quinto variou em sequência (80–8F e retorno). A hipótese é contador/estado, ainda sem decodificação comprovada.
- O último byte varia de modo determinístico, mas o algoritmo de CRC/checksum não foi identificado. Testes com CRC-8 polinômios 0x07 e 0x1D não forneceram correspondência consistente.
- Repetir `0x0DE` não desbloqueou a assistência.
- Uma varredura anterior de IDs diagnósticos padrão 0x600–0x7FF com sessões UDS 10 01/10 03 não encontrou resposta.
- Movimentar o eixo de batente a batente não revelou ID novo claramente associado ao movimento.

Há menções históricas a quadros estendidos como `0x1E360002` e `0x1E114000`; não devem ser tratados como tráfego validado da unidade. No firmware atual, vários IDs/payloads de outros módulos são transmitidos como emulação experimental de C-CAN. Não são prova de mensagens reais do veículo.

## UDS / ISO-TP: avanço importante

IDs estendidos configurados no sketch que respondeu:
- Tester TX: `0x18DA30F1`
- EPS RX: `0x18DAF130`

O firmware usa MCP2515 em CAN 500 kbit/s, tratamento ISO-TP de Single Frame, First Frame e Consecutive Frame, e envio de Flow Control.

Log reportado pelo usuário:
```text
[UDS] Consultando quantidade de DTCs (19 01)...
RAW RX [0x18DAF130]: 06 59 01 FF 00 00 13
```

Interpretação conforme a estrutura UDS de `ReadDTCInformation/reportNumberOfDTCByStatusMask (0x19 0x01)`:
- `06`: ISO-TP Single Frame com seis bytes UDS.
- `59`: resposta positiva ao serviço `19`.
- `01`: subfunção reportNumberOfDTCByStatusMask.
- `FF`: DTCStatusAvailabilityMask.
- `00`: DTCFormatIdentifier (formato 00).
- `00 13`: DTCCount = 0x0013 = 19.

Portanto, o resultado indica 19 DTCs que correspondem ao filtro/status mask enviado (`FF`), de acordo com a resposta da ECU. **Não equivale a 19 códigos individuais decodificados.** A consulta `19 02` precisa ser validada e seus registros analisados antes de listar códigos.

Referência de estrutura do serviço: https://uds.readthedocs.io/en/stable/pages/knowledge_base/service.html

## Firmware existente — observações importantes

Leia o arquivo inteiro `firmware/argo_eps_last_experiment.ino` antes de editar. Ele tem 334 linhas e deve permanecer preservado como registro do experimento que produziu a resposta descrita.

O código atual:
- Solicita sessão estendida `10 03`.
- Envia solicitação de limpeza de DTCs `14 FF FF FF` automaticamente.
- Consulta `19 01 FF` e depois `19 02 FF`.
- Usa os IDs UDS estendidos citados acima.
- Emula em background diversos IDs CAN de BCM, BSM, motor, ABS, painel, SAS, embreagem, freio de mão e uma mensagem Proxi.
- Inclui parser ISO-TP e impressão dos quadros brutos.

**Atenção:** a limpeza automática de DTCs pode apagar evidências úteis. Uma nova versão deve ser somente leitura por padrão e nunca limpar DTC automaticamente. O tráfego emulado deve ficar desligado por padrão e ser separado do modo de captura passiva.

## O que ainda falta descobrir

- Quais mensagens reais do carro doador são necessárias para que a EPS aceite estado operacional.
- Se a EPS exige autenticação, configuração Proxi, gateway/body computer ou condições de segurança.
- Mensagem real de velocidade e estado de ignição/motor.
- Significado dos bytes do ID `0x0DE`.
- Interpretação integral dos registros `19 02`.
- Função elétrica dos fios que não pertencem ao par CAN e ao pino de ativação, com alimentação principal verificada por diagrama.
- Se e como a unidade pode operar sem o Body Computer original.

Não inventar IDs, payloads, CRCs, frequência, dados de velocidade, frames de ignição, chave de segurança ou sequência de desbloqueio. Separar claramente fato medido, hipótese e experimento pendente.

## Pedido de trabalho para Claude

Atue como engenheiro de firmware embarcado e diagnóstico automotivo. Leia o repositório e o sketch original integralmente antes de propor alterações.

1. Faça revisão estática detalhada do sketch atual, apontando bugs e riscos com referência a funções/linhas.
2. Verifique especialmente o parser ISO-TP: validação de DLC e PCI, tamanho de Single Frame, tamanho anunciado no First Frame, limite do buffer, sequência de Consecutive Frame, timeout, Flow Control e proteção contra leitura/escrita fora do buffer.
3. Verifique o parser UDS: comprimento mínimo por subfunção, resposta negativa `7F`, resposta pendente `78`, serviço inesperado e resposta positiva.
4. Para `19 01`, trate o formato da resposta corretamente: status availability mask, DTC format identifier e contador de 16 bits. O log recebido corresponde a `FF 00 00 13` após `59 01`, ou seja, availability `FF`, formato `00`, contador `0x0013`.
5. Para `19 02`, valide estrutura, tamanho dos registros, status bytes e fragmentação ISO-TP antes de decodificar qualquer DTC. Não invente descrições de falha.
6. Reestruture o diagnóstico para aguardar a resposta esperada antes de avançar, com timeout e logs claros.
7. Crie firmware novo em arquivo separado; mantenha o sketch original intocado. Modos distintos: captura passiva sem transmissão e diagnóstico UDS somente leitura. Emulação CAN desativada por padrão.
8. Faça testes unitários ou um harness em PC para parser ISO-TP/UDS, incluindo frames truncados, comprimento inválido, sequência incorreta, timeout, resposta positiva, resposta negativa e DTC record truncado.
9. Produza relatório explicando cada correção, pressupostos, limitações do Arduino Uno/MCP2515, como compilar e como validar em bancada.
10. Não declare desbloqueio da coluna. Não implemente busca por força bruta de bytes, comando de torque ou acionamento do motor. Priorize aquisição passiva, leitura diagnóstica e engenharia reproduzível.

## Segurança obrigatória

A direção é um sistema crítico de segurança. Desenvolver e validar somente em bancada controlada, com fonte protegida, fusível, limitação de corrente, proteção contra polaridade incorreta e botão de corte de energia. Não orientar ensaio em via pública nem enviar quadros não validados para movimentar o motor de direção. Se a próxima descoberta exigir uma captura de veículo doador, explicar exatamente que dados passivos devem ser registrados, sem sugerir transmissão de comandos desconhecidos.

## Entrega

- Uma nova versão do firmware em arquivo separado.
- Testes reproduzíveis do parser.
- Relatório de revisão e lista de incertezas.
- Pull request para revisão humana, sem merge automático.

Comece listando as incertezas específicas desta unidade e os riscos do código original. Não escreva uma sequência de desbloqueio especulativa.
