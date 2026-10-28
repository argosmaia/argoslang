# Diagnósticos da ArgosLang

Este capítulo reúne o formato das mensagens e o catálogo de chamadas de
função. Para os comandos do compilador, consulte
[Ferramentas e diagnósticos](08-ferramentas-e-diagnosticos.md); para erros de
opcionais e arrays, consulte [null-safety](11-null-safety-e-opcionais.md#diagnosticos-erros-opcionais).

## Índice

- [Sistema de diagnósticos ArgosLang](#diagnosticos)
- [Diagnósticos semânticos: chamadas de função](#chamadas)

<a id="diagnosticos"></a>

<a id="diagnosticos-sistema-de-diagnósticos-argoslang"></a>

## Sistema de diagnósticos ArgosLang

Todo diagnóstico tem a forma `ARGOS-XNNNN`:

| Prefixo | Fase | Exemplos |
| --- | --- | --- |
| `L` | análise léxica | UTF-8 inválido, token inválido, comentário não fechado |
| `P` | parser e gramática | delimitador ausente, identificador inválido, palavra-chave mal escrita |
| `S` | análise semântica | tipo, escopo, inicialização, chamadas e mutabilidade |
| `C` | lowering, VM e compilação nativa | construção ainda não baixável, `main` inválida |
| `W` | compatibilidade e práticas suspeitas | grafia legada, recurso obsoleto |

Os quatro dígitos são estáveis. O grupo `000x` reserva falhas fundamentais de
cada fase; famílias posteriores agrupam recursos, como `S034x` para chamadas.
Um código não muda de significado depois de publicado.

O catálogo central é dividido em subfamílias, não em um arquivo por tipo de
erro: `catalog::lexer`, `catalog::parser`, `catalog::compiler`,
`catalog::warning`, `catalog::semantic::symbols`,
`catalog::semantic::declarations`, `catalog::semantic::control`,
`catalog::semantic::types` e `catalog::semantic::calls`. Isso mantém a origem
da falha clara e permite importar só a família necessária em cada fase.

<a id="diagnosticos-severidades"></a>

### Severidades

- `error`: impede a execução ou compilação da unidade.
- `warning`: o programa continua, mas existe uma prática suspeita ou recurso
  obsoleto.
- `note`: contexto adicional ligado a outro diagnóstico.

As mensagens aparecem com um título curto, um detalhe do trecho, uma explicação
em linguagem simples e uma sugestão de correção. `func` e nomes de tipos com
inicial maiúscula não são aceitos. O apelido antigo `null` ainda gera
`ARGOS-W0002`; prefira `nil`.

As fases importam definições de
`compiler/include/argos/diagnostics/diagnostic_catalog.hpp` e emitem por
`DiagnosticEngine::emit`. Cada definição fornece código, severidade, título,
explicação e sugestão. A mensagem emitida pode acrescentar o nome de uma
variável, tipo ou rótulo, sem alterar o contrato do código.

<a id="diagnosticos-erros-comuns-de-sintaxe"></a>

### Erros comuns de sintaxe

| Código | Caso | Exemplo inválido |
| --- | --- | --- |
| `ARGOS-P0001` | sequência sintática inesperada | `function main( {` |
| `ARGOS-P0002` | identificador iniciado por número | `var 1total: int = 0` |
| `ARGOS-P0003` | palavra reservada usada como nome | `var function: int = 0` |
| `ARGOS-P0004` | keyword incompleta/desconhecida | `functio main() -> void {}` |

Delimitadores seguem a gramática: `()` delimitam argumentos, parâmetros e
agrupamento; `{}` delimitam blocos; `[]` são reservados para coleções e acesso
indexado. O parser informa o token e a posição esperados quando eles aparecem
em ordem errada ou sem fechamento.


---

<a id="chamadas"></a>

<a id="chamadas-diagnósticos-semânticos-chamadas-de-função"></a>

## Diagnósticos semânticos: chamadas de função

Este catálogo documenta a família `ARGOS-S034x`, emitida durante a resolução
de chamadas. As definições reutilizáveis ficam em
`compiler/include/argos/diagnostics/diagnostic_catalog.hpp`, na seção
`catalog::semantic::calls`.

| Código | Nome | Condição | Direção de correção |
| --- | --- | --- | --- |
| `ARGOS-S0340` | alvo de chamada não é chamável | O valor antes de `(...)` não tem tipo de função não opcional. | Use uma função nomeada, lambda ou closure; desembrulhe funções opcionais antes de chamar. |
| `ARGOS-S0341` | função não declarada | O identificador não resolve para uma função visível. | Declare a função ou corrija o nome. |
| `ARGOS-S0342` | argumento obrigatório ausente | Faltou valor para algum parâmetro. | Forneça o argumento indicado. |
| `ARGOS-S0343` | tipo de argumento incompatível | O tipo do argumento não é atribuível ao parâmetro. | Passe um valor compatível. |
| `ARGOS-S0344` | argumento posicional sem parâmetro posicional | A posição exige argumento nomeado. | Use o rótulo correto. |
| `ARGOS-S0345` | rótulo de argumento inexistente | O rótulo não pertence à assinatura. | Consulte os rótulos da função. |
| `ARGOS-S0346` | parâmetro preenchido duas vezes | Um parâmetro recebeu dois argumentos. | Remova ou renomeie o argumento repetido. |
| `ARGOS-S0347` | rótulo de parâmetro duplicado | Uma assinatura declara o mesmo rótulo duas vezes. | Use rótulos externos únicos. |

<a id="chamadas-uso-no-compilador"></a>

### Uso no compilador

Em vez de espalhar literais de código e mensagem, importe o catálogo:

```cpp
#include "argos/diagnostics/diagnostic_catalog.hpp"

using argos::diagnostics::catalog::semantic::calls::non_named_callee;
report(non_named_callee.code, span, std::string{non_named_callee.title});
```

Mensagens podem incluir contexto local, como o nome da função ou do rótulo,
mas não devem mudar o código nem o significado documentado dele.
