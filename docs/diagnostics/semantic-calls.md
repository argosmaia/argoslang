# Diagnósticos semânticos: chamadas de função

Este catálogo documenta a família `ARGOS-S034x`, emitida durante a resolução
de chamadas. As definições reutilizáveis ficam em
`compiler/include/argos/diagnostics/diagnostic_catalog.hpp`, na seção
`catalog::semantic::calls`.

| Código | Nome | Condição | Direção de correção |
| --- | --- | --- | --- |
| `ARGOS-S0340` | alvo de chamada não é uma função nomeada | O valor antes de `(...)` não é um identificador de função. | Declare uma `function` nomeada e chame-a diretamente. |
| `ARGOS-S0341` | função não declarada | O identificador não resolve para uma função visível. | Declare a função ou corrija o nome. |
| `ARGOS-S0342` | argumento obrigatório ausente | Faltou valor para algum parâmetro. | Forneça o argumento indicado. |
| `ARGOS-S0343` | tipo de argumento incompatível | O tipo do argumento não é atribuível ao parâmetro. | Passe um valor compatível. |
| `ARGOS-S0344` | argumento posicional sem parâmetro posicional | A posição exige argumento nomeado. | Use o rótulo correto. |
| `ARGOS-S0345` | rótulo de argumento inexistente | O rótulo não pertence à assinatura. | Consulte os rótulos da função. |
| `ARGOS-S0346` | parâmetro preenchido duas vezes | Um parâmetro recebeu dois argumentos. | Remova ou renomeie o argumento repetido. |
| `ARGOS-S0347` | rótulo de parâmetro duplicado | Uma assinatura declara o mesmo rótulo duas vezes. | Use rótulos externos únicos. |

## Uso no compilador

Em vez de espalhar literais de código e mensagem, importe o catálogo:

```cpp
#include "argos/diagnostics/diagnostic_catalog.hpp"

using argos::diagnostics::catalog::semantic::calls::non_named_callee;
report(non_named_callee.code, span, std::string{non_named_callee.title});
```

Mensagens podem incluir contexto local, como o nome da função ou do rótulo,
mas não devem mudar o código nem o significado documentado dele.
