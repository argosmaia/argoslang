# Interpretar programas ArgosLang

**Estado atual:** `argosc run` executa pela VM o mesmo subconjunto inicial suportado pelo backend nativo: funções sem parâmetros que retornam `int` ou `bool`, variáveis locais, aritmética, comparações, atribuição, `if`, `while`, chamadas locais sem argumentos e `return`. Alguns recursos aceitos por `check`, como opcionais, protocolos e `string`, ainda não possuem lowering para a VM.

## Compilar não é interpretar

Estes dois fluxos têm saídas diferentes:

| Objetivo | Comando | Disponibilidade |
| --- | --- | --- |
| Validar o fonte | `argosc check arquivo.argos` | Disponível |
| Gerar um binário do host | `argosc arquivo.argos` | Disponível para o subconjunto x86-64/Linux |
| Rodar o binário gerado | `./arquivo` | Disponível após compilação bem-sucedida |
| Interpretar pela VM | `argosc run arquivo.argos` | Disponível para o subconjunto executável |

`argosc arquivo.argos` **não** interpreta: produz um executável nativo. `./arquivo` executa esse binário sem ler novamente o fonte `.argos`. Já `argosc run arquivo.argos` executa em memória, sem criar executável.

## O que fazer hoje

Construa o `argosc` conforme [compilar.md](compilar.md) e verifique um programa:

```bash
./build/bin/argosc check examples/minimal_literal.argos
```

Isso confirma apenas a validade do frontend. `ast` imprime a árvore sintática, não avalia expressões:

```bash
./build/bin/argosc ast examples/minimal_literal.argos
```

Para executar pela VM, use:

```bash
./build/bin/argosc run examples/minimal_literal.argos
```

A saída é `Resultado da VM: 42`; o processo termina com status zero quando a execução é bem-sucedida. O valor de `main()` é impresso, em vez de ser usado como status do processo, para preservar resultados fora de `0..255`.

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

No código atual, a rota executável gera IR limitada e a VM interpreta os opcodes dessa fatia diretamente. A VM valida índices de funções, blocos, registradores, slots e constantes antes de usá-los. Divisão por zero e a divisão excepcional `Int.min / -1` interrompem a execução com erro de runtime; a política geral de overflow continua em evolução.

## Fluxo pretendido para `run`

`argosc run arquivo.argos` valida o fonte, gera a IR executável, inicia a VM e executa a entrada sem produzir um executável nativo no diretório atual. Os testes de ponta a ponta cobrem literal (`42`), aritmética (`14`), desvio (`7`), laço (`6`), chamadas, blocos aninhados e aritmética com sinal. O suporte a opcionais pela VM continua futuro.

```bash
argosc run examples/minimal_literal.argos
```

## REPL

`argosc repl` abre um interpretador interativo de expressões `int`. Ele não cria arquivos e aceita `:help`, `:quit` ou `:q`:

```text
$ argosc repl
>>> 6 * 7
42
>>> :quit
```
