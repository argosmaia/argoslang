# ArgosLang MVP

O MVP é uma linguagem compilada e interpretável para programas inteiros em
x86-64/Linux. A sintaxe canônica dos tipos primitivos é sempre minúscula:
`int`, `bool`, `byte`, `char`, `float`, `double`, `string` e `void`. A declaração de função usa
`function`. As grafias antigas `func` e os nomes de tipos com inicial maiúscula
não são aceitos; tipos e palavras-chave seguem a forma minúscula.

Este documento descreve o que o código atual consegue executar. Para ver o que
falta para tornar essa primeira versão mais ampla e confiável, consulte o
[roteiro de consolidação do MVP](mvp-roadmap.md).

## Subconjunto executável

O núcleo abaixo é complementado pelo suporte a
[POO e funções como valores](#poo-e-funções-como-valores-26-de-outubro-de-2026).

`run`, `compile` e o REPL usam o mesmo lowering para IR tipada e validada. A VM
executa essa IR diretamente; o backend x86-64/Linux também a consome. `check`
valida recursos adicionais do frontend que ainda não têm lowering executável:

- funções com retorno `int`, `bool`, `float`, `double`, `string` ou `void`, com
  até seis parâmetros `int`/`bool`/`float`/`double`/`string`;
- variáveis locais `var` (imutáveis por padrão) e `var mut` (reatribuíveis);
- declarações tipadas sem inicializador; uma variável imutável pode receber
  seu primeiro valor depois da declaração, mas não pode ser alterada novamente;
- literais inteiros, booleanos, reais e strings;
- aritmética inteira e real (`/` sempre retorna `double`), comparações,
  negação, igualdade de strings por conteúdo, atribuição, `if`, `while`,
  `break`, `continue` e `return`;
- operadores booleanos `&&` e `||` com avaliação de curto-circuito: o lado
  direito só é executado quando necessário;
- chamadas locais posicionais ou nomeadas de até seis argumentos;
- `print` de valores `int`, `bool`, `double` e `string`, incluindo variáveis,
  parâmetros e retornos de função;
- entrada `function main() -> int` ou `function main() -> void`, sem parâmetros.

Strings podem conter UTF-8, escapes reconhecidos pelo lexer e quebras de linha
em literais de três aspas. A igualdade compara os bytes do texto; `print`
escreve os bytes sem adicionar separadores entre argumentos e termina com uma
quebra de linha. Ainda não há concatenação de strings nem suporte a `char`
executável.

`nil` representa ausência em `T?`. Opcionais, inclusive camadas aninhadas,
executam em arquivos na VM e no nativo: `??`, `?==`, `?=`, `??=`, `!!`, `!!=` e
`?.` de classes concretas. Arrays homogêneos `[T]` têm literais, leitura/escrita
`[]` e indexação `?[` de arrays opcionais, inspirada no Dart: ausência omite a
avaliação do índice; índice inválido em array presente é erro. Arrays têm
tamanho fixado na criação, identidade por referência e escrita por base mutável.

`!!`/`!!=` sobre ausência e índices fora dos limites encerram com status 70,
função, linha e coluna. A análise rejeita unwrap implícito, `void?`, alvos
imutáveis, leituras antes da inicialização e inicialização somente em ramos
que podem ser pulados. Consulte o [contrato de null-safety](guide/11-null-safety-e-opcionais.md).

Tipos como `byte`, `char` e os demais inteiros estreitos ou sem sinal ainda não
são executáveis. `float` e `double` usam IEEE-754 binary64.

## Estado atual do pipeline

- O parser atual é manual, com precedência de operadores; LR(1) continua sendo
  uma direção de arquitetura, não a implementação presente.
- A IR possui tipos de registrador/slot e um validador compartilhado pelas
  fronteiras da VM e do backend nativo.
- A VM executa a IR diretamente. `BytecodeEmitter` ainda é uma representação
  textual de inspeção, não um formato de bytecode carregável.
- O backend nativo gera assembly x86-64 e usa `cc` para montar e ligar.
- O REPL compila cada entrada pela mesma semântica, IR e VM, mantendo valores
  locais `int`/`bool`/`float`/`double` e seus opcionais, inclusive aninhados, entre entradas. Literais string podem ser
  impressos, mas variáveis string ainda não são preservadas entre entradas.
  Não é um interpretador AST paralelo.
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

## POO e funções como valores (26 de outubro de 2026)

Arquivos também podem executar, pela VM e pelo backend x86-64/Linux:

- classes concretas sem bases, campos tipados e identidade por referência;
- campos `var` inicializados uma vez em `construct`, campos `var mut` e valores
  padrão; sem `construct` explícito, todos os campos exigem inicializador;
- construtores `construct(...)`, criação com `new Tipo(...)`, métodos com
  `this` e acesso implícito a campos;
- propriedades computadas tipadas com `getter()` e `setter()`/`setter(nome)`,
  leitura por acesso a membro e escrita por atribuição simples ou composta;
- visibilidade `public`, `private`, `protected` e `internal` (um módulo atual);
- argumentos nomeados, avaliados na ordem escrita e associados pelos rótulos;
- tipos de função `(int) -> int`, funções como argumentos e retornos, lambdas
  `(x: int) => x + 1` e chamadas indiretas;
- closures com captura por valor de bindings locais imutáveis já inicializados,
  incluindo closures retornadas e aninhadas. Uma referência a objeto capturada
  conserva a identidade do objeto; seus campos mutáveis continuam alteráveis.

Funções comuns aceitam até seis argumentos. Métodos e construtores reservam
uma posição para `this`; chamadas indiretas e lambdas reservam uma para o
ambiente, deixando até cinco argumentos explícitos. Não há sobrecarga nem
argumentos padrão de parâmetros. Parâmetros de lambdas exigem tipo explícito;
o retorno é inferido em corpos de expressão e assume `void` em corpos de bloco
sem anotação. Não há captura mutável explícita nesta etapa.

Objetos e ambientes usam uma arena com duração da execução, sem GC. No nativo,
a arena é liberada no encerramento do processo. Ainda não há destruição por
escopo, `deinit` ou verificação de ownership; este modelo não é uma implementação
da memória determinística planejada na gramática.

`check` também valida interfaces de métodos e propriedades, classes abstratas, herança simples,
conformidade múltipla, ciclos, bases `final`, métodos `virtual`, requisitos
`abstract`, `override` e `final override`. A execução de classes com bases ou
classes abstratas é recusada com `ARGOS-C0001`: despacho polimórfico ainda não
tem lowering. Referências ligadas a métodos (`var f = objeto.metodo`) também
não são executáveis; a chamada `objeto.metodo(...)` funciona. Interfaces aceitam
contratos `getter();`/`setter();`, mas não implementações padrão. `static`,
`sealed`, genéricos e `super` continuam pendentes.

O REPL pode chamar funções de ordem superior em uma entrada, mas não preserva
objetos nem closures em variáveis entre entradas e rejeita essas declarações
explicitamente. Classes não são declarações persistentes do REPL.

Exemplos de conformidade: `minimal_objects.argos`, `minimal_closures.argos` e
`minimal_object_closure.argos` e `minimal_properties.argos`, com testes de `check`, VM, executável nativo e
assembly montado externamente.


A partir de 27/10/2026, `init(...)` é rejeitado: o único nome de construtor é
`construct(...)`, sem compatibilidade com a sintaxe antiga. Propriedades exigem
um getter com corpo; setters exigem `var mut` e usam o parâmetro implícito
`value` ou o nome escrito em `setter(nome)`. Não há acessores automáticos nem
visibilidade independente por acessor. Propriedades sem setter são somente
leitura. Elas não exigem inicialização em `construct`, pois não armazenam estado.


## Null-safety e arrays opcionais (28 de outubro de 2026)

A implementação cobre presença/ausência na IR, VM e x86-64, valores em
parâmetros/retornos/campos/capturas, curto-circuito e igualdade de camadas.
`?.` e `?[` tornam o resultado opcional conservando membros/elementos já
opcionais. `T??` declarado explicitamente mantém estados distintos de ausência
externa e presença com ausência interna. Funções opcionais precisam de unwrap
antes da chamada; métodos podem ser chamados por `obj?.metodo(...)`.

O REPL preserva opcionais numéricos/bool e o estado anterior após entradas
com erro. Continua rejeitando persistência de strings, objetos, arrays e
closures. Arrays multidimensionais, redimensionamento, slices/views, `[T; N]`
e `for` permanecem pendentes. Herança e protocolos continuam fora do lowering.

Exemplos de conformidade: `minimal_null_safety.argos`,
`minimal_optional_arrays.argos`, `minimal_nil_unwrap.argos` e
`minimal_array_bounds.argos`. Consulte o [roadmap](mvp-roadmap.md) para o restante
do MVP e o [capítulo 11](guide/11-null-safety-e-opcionais.md) para as regras.
