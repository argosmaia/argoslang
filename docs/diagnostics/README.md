# Sistema de diagnósticos ArgosLang

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

## Severidades

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

## Erros comuns de sintaxe

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
