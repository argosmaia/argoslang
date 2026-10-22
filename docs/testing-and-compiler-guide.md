# Testar a ArgosLang e o compilador

Este guia descreve o pipeline que existe hoje. A sintaxe canônica usa
`function`, `var`/`var mut`, `nil` e tipos primitivos em minúsculas. O escopo
executável está detalhado em [mvp.md](mvp.md).

## 1. Construir e testar

Na raiz do repositório, configure o projeto, compile o compilador e execute a
suíte:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

`argosc` será criado em `build/bin/argosc`. A primeira configuração com testes
pode baixar Catch2.

## 2. Inspecionar um programa

Os comandos `lex`, `parse`, `ast` e `check` mostram as etapas do frontend:

```bash
./build/bin/argosc lex examples/minimal_arithmetic.argos
./build/bin/argosc parse examples/minimal_arithmetic.argos
./build/bin/argosc ast examples/minimal_arithmetic.argos
./build/bin/argosc check examples/minimal_arithmetic.argos
```

`check` confirma sintaxe e semântica; não executa o programa. Quando um erro
ocorre, a saída informa arquivo, linha, coluna, explicação em linguagem simples
e uma sugestão para corrigir.

## 3. Executar e compilar

O mesmo exemplo pode ser executado na VM e compilado para x86-64/Linux:

```bash
./build/bin/argosc run examples/minimal_print_int.argos
./build/bin/argosc compile examples/minimal_print_int.argos -o /tmp/argos-print
/tmp/argos-print
```

As duas rotas imprimem `42`. `run` também mostra `Resultado da VM: 0`; no
executável nativo, o retorno de `main()` aparece no status do processo. Esse
status preserva somente os oito bits inferiores em Unix.

Um exemplo mínimo usa a forma canônica:

```argos
function main() -> int {
    var answer: int = 6 * 7
    print(answer)
    return 0
}
```

`var` é imutável por padrão. Use `var mut` quando o valor precisar mudar.
Chamadas, laços, `break` e `continue` funcionam nas duas rotas para o
subconjunto descrito em [mvp.md](mvp.md).

## 4. Estado atual das fases

- O parser em execução é manual, com precedência de operadores; LR(1) é uma
  direção futura documentada, não o parser atual.
- A semântica alimenta uma IR com tipos explícitos e validação compartilhada
  pela VM e pelo backend nativo.
- A VM executa essa IR diretamente. `BytecodeEmitter` ainda imprime texto de
  inspeção; não existe formato de bytecode independente.
- O backend disponível emite assembly x86-64 e usa `cc` para montar e ligar.
- O REPL usa o mesmo frontend, lowering e VM, e preserva variáveis locais
  `int`/`bool` entre entradas.
- `string` variável, opcionais, protocolos, backends ARM64/RISC-V/AVR,
  bibliotecas padrão e cross-compilation continuam fora do MVP executável.

Uma construção aceita por `check` ainda pode não ser executável. Nesses casos,
o compilador informa o diagnóstico `ARGOS-C0001`, em vez de produzir um
executável incompleto.
