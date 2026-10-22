# Interpretar programas ArgosLang

**Estado atual:** `argosc run`, `argosc compile` e o REPL compartilham o mesmo subconjunto executável. Ele inclui funções com até seis parâmetros `int`/`bool`/`float`/`double`, variáveis locais, inclusive declarações tipadas sem inicializador, aritmética inteira e real, comparações, atribuição, `if`, `while`, `break`, `continue`, chamadas posicionais, `return` e `print` de `int`, `bool`, `float`, `double` e literais `string`. A operação `/` sempre retorna `double`, inclusive quando os operandos são `int`. `float` e `double` usam binary64 nesta versão. Valores `string` armazenados, opcionais e protocolos ainda não têm lowering executável.

## Compilar não é interpretar

Estes dois fluxos têm saídas diferentes:

| Objetivo | Comando | Disponibilidade |
| --- | --- | --- |
| Validar o fonte | `argosc check arquivo.argos` | Disponível |
| Gerar um binário do host | `argosc arquivo.argos` | Disponível para o subconjunto x86-64/Linux |
| Rodar o binário gerado | `./arquivo` | Disponível após compilação bem-sucedida |
| Executar pela VM | `argosc run arquivo.argos` | Disponível para o subconjunto executável |

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

A arquitetura em [architecture.md](../architecture.md) separa o frontend das
rotas de execução:

```text
.argos -> lexer -> parser -> semântica -> IR tipada e validada
       -> VM (direto da IR) ou backend nativo x86-64/Linux
```

No código atual, a VM interpreta os opcodes da IR diretamente. Um validador
compartilhado verifica funções, blocos, registradores, slots, chamadas e tipos
antes da execução ou da geração nativa. Ainda não existe bytecode binário
separado; `BytecodeEmitter` é somente uma visualização textual. O operador
`%` permanece inteiro; divisor zero e `int.min % -1` são erros de runtime.

## Fluxo pretendido para `run`

`argosc run arquivo.argos` valida o fonte, gera a IR executável, inicia a VM e executa a entrada sem produzir um executável nativo no diretório atual. Os testes de ponta a ponta cobrem literal, divisão real, aritmética, desvios, laços, `break`, `continue`, chamadas, `print`, erros de módulo e equivalência VM/nativo. O suporte a opcionais pela VM continua futuro.

```bash
argosc run examples/minimal_literal.argos
```

## REPL

`argosc repl` usa a mesma IR e VM, mantendo valores `int`/`bool`/`float`/`double` e funções
entre entradas. Ele não cria arquivos e aceita `:help`, `:reset`, `:quit` ou
`:q`:

```text
$ argosc repl
>>> 6 * 7
42
>>> :quit
```
