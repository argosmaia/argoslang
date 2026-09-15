# Interpretar programas ArgosLang

**Estado atual:** não existe interpretador utilizável nem comando `argosc run`. Alguns `examples/minimal_*.argos` já podem ser executados como binários nativos em x86-64/Linux, mas isso não é interpretação pela VM.

## Compilar não é interpretar

Estes dois fluxos têm saídas diferentes:

| Objetivo | Comando | Disponibilidade |
| --- | --- | --- |
| Validar o fonte | `argosc check arquivo.argos` | Disponível |
| Gerar um binário do host | `argosc arquivo.argos` | Disponível para o subconjunto x86-64/Linux |
| Rodar o binário gerado | `./arquivo` | Disponível após compilação bem-sucedida |
| Interpretar pela VM | `argosc run arquivo.argos` | Ainda não implementado |

`argosc arquivo.argos` **não** interpreta: produz um executável nativo. `./arquivo` executa esse binário sem ler novamente o fonte `.argos`.

## O que fazer hoje

Construa o `argosc` conforme [compilar.md](compilar.md) e verifique um programa:

```bash
./build/bin/argosc check examples/minimal_literal.argos
```

Isso confirma apenas a validade do frontend. `ast` imprime a árvore sintática, não avalia expressões:

```bash
./build/bin/argosc ast examples/minimal_literal.argos
```

Não tente `argosc run arquivo.argos`: o CLI responderá que o comando é desconhecido.

Para observar o resultado de um programa do subconjunto sem esperar pela VM, compile e rode como em [compilar.md](compilar.md):

```bash
./build/bin/argosc examples/minimal_literal.argos
./minimal_literal
echo $?
```

O status esperado é `42`; isso é **execução nativa**, não interpretação. Programas com opcionais ou protocolos podem passar em `check` e ainda assim não ter binário nem suporte de VM.

## Como a interpretação deve funcionar

A arquitetura em [architecture.md](../architecture.md) prevê um único frontend para compilação e interpretação:

```text
.argos -> lexer -> parser -> semântica -> IR Argos verificada
       -> bytecode verificado -> VM -> resultado de main()
```

A VM deverá carregar funções, criar frames, guardar valores tipados, executar saltos e chamadas, e preservar a semântica de opcionais e erros de runtime. O bytecode externo deverá ser validado antes de rodar. O modelo de valores, as chamadas e a ABI da VM ainda precisam ser fechados antes de anunciar um formato de bytecode estável.

No código atual, a rota executável nativa possui lowering limitado para IR, mas `VM::run()` continua um stub. Há um emissor textual de opcodes, que não constitui um interpretador. Resultados dos exemplos nativos foram medidos pelo status dos processos; a execução via VM continua futura.

## Fluxo pretendido para `run`

O comando esperado é `argosc run arquivo.argos`: ele deverá validar o fonte, gerar ou carregar bytecode, iniciar a VM e executar a entrada sem produzir um executável nativo no diretório atual. O primeiro teste de ponta a ponta deverá executar `examples/minimal_literal.argos` e comparar o resultado `42`; depois, aritmética (`14`), desvio (`7`), laço (`6`) e opcional (`9`). O comando abaixo é **ilustrativo, não executável hoje**:

```text
argosc run examples/minimal_literal.argos
```

O contrato de saída do processo ainda deve definir se o valor de `main()` será impresso, usado como código de saída, ou ambos. Não assuma que o status do processo reproduzirá diretamente qualquer inteiro retornado por `main()`.
