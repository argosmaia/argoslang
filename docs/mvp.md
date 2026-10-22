# ArgosLang MVP

O MVP é uma linguagem compilada e interpretável para programas inteiros em
x86-64/Linux. A sintaxe canônica dos tipos primitivos é sempre minúscula:
`int`, `bool`, `byte`, `char`, `float`, `double`, `string` e `void`. A declaração de função usa
`function`; `func` é aceito apenas como compatibilidade temporária. Grafias antigas com inicial
maiúscula são aceitas apenas para compatibilidade do frontend e não devem ser
usadas em código novo.

## Subconjunto executável

`run`, `compile` e o REPL usam o mesmo lowering para IR tipada e validada. A VM
executa essa IR diretamente; o backend x86-64/Linux também a consome. `check`
valida recursos adicionais do frontend que ainda não têm lowering executável:

- funções com retorno `int`, `bool`, `float`, `double` ou `void`, com até seis
  parâmetros `int`/`bool`/`float`/`double`;
- variáveis locais `var` (imutáveis por padrão) e `var mut` (reatribuíveis);
- declarações tipadas sem inicializador; uma variável imutável pode receber
  seu primeiro valor depois da declaração, mas não pode ser alterada novamente;
- literais inteiros, booleanos e reais;
- aritmética inteira e real (`/` sempre retorna `double`), comparações, negação,
  atribuição, `if`, `while`,
  `break`, `continue` e `return`;
- chamadas locais posicionais de até seis argumentos;
- `print` de valores `int`, `bool`, `double` e literais `string`;
- entrada `function main() -> int` ou `function main() -> void`, sem parâmetros.

`nil` representa a ausência de um valor opcional. O frontend reconhece
opcionais, protocolos e argumentos nomeados, mas esses recursos não fazem parte
da rota executável do MVP. Tipos como `byte`, `char`, números reais e variáveis
`string` também não podem ser armazenados na IR executável. `float` e `double`
são executados atualmente com representação IEEE-754 binary64.

## Estado atual do pipeline

- O parser atual é manual, com precedência de operadores; LR(1) continua sendo
  uma direção de arquitetura, não a implementação presente.
- A IR possui tipos de registrador/slot e um validador compartilhado pelas
  fronteiras da VM e do backend nativo.
- A VM executa a IR diretamente. `BytecodeEmitter` ainda é uma representação
  textual de inspeção, não um formato de bytecode carregável.
- O backend nativo gera assembly x86-64 e usa `cc` para montar e ligar.
- O REPL compila cada entrada pela mesma semântica, IR e VM, mantendo valores
  locais `int`/`bool`/`float`/`double` entre entradas. Não é um interpretador AST
  paralelo.
- `/` é divisão real, inclusive com operandos `int`; `%` continua inteiro e
  informa erro de runtime por divisor zero ou `int.min % -1`.

## Comandos

```bash
# validação estática
argosc check examples/minimal_parameters.argos

# execução pela VM, sem criar um arquivo de saída
argosc run examples/minimal_parameters.argos

# executável nativo x86-64/Linux
argosc compile examples/minimal_parameters.argos -o parameters
./parameters

# REPL com a mesma VM e estado entre entradas
argosc repl
```

As saídas de `print` são iguais em `run` e no binário compilado. Sem `print`, a
VM escreve `Resultado da VM: N`; no binário nativo, o retorno de `main` é o
status do processo e o Unix preserva somente seus oito bits inferiores.
