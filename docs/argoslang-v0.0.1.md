# ArgosLang v0.0.1

> Para estudar a linguagem por capítulos, comece no
> [Guia da Linguagem ArgosLang](guide/README.md). Este arquivo permanece como
> referência compacta da versão e mapa de implementação.

ArgosLang v0.0.1 é uma versão inicial de uma linguagem estaticamente tipada,
com compilação nativa x86-64/Linux e execução por VM para o subconjunto
executável. Esta referência descreve o estado implementado, não a linguagem
planejada em sua totalidade.

## Instalação e build

No Ubuntu/Pop!_OS, instale CMake, Ninja, um compilador C++23 e ICU:

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build g++ libicu-dev libreadline-dev
```

Na raiz do repositório:

```bash
cmake -S . -B build -G Ninja \
  -DARGOS_BUILD_TESTS=OFF \
  -DCMAKE_CXX_COMPILER=g++
cmake --build build
```

O compilador fica em `./build/bin/argosc`.

## Usar o compilador no terminal

Para usar `argosc` sem escrever o caminho completo, adicione a pasta do build
ao `PATH` da sessão atual:

```bash
export PATH="$PATH:$HOME/Projetos/argoslang/build/bin"
```

Para tornar a alteração permanente no Bash:

```bash
echo 'export PATH="$PATH:$HOME/Projetos/argoslang/build/bin"' >> ~/.bashrc
source ~/.bashrc
argosc --help
```

Se o repositório estiver em outro local, substitua o caminho corretamente.
Você também pode continuar usando `./build/bin/argosc` na raiz do projeto.

## Primeiro programa

Crie `hello.argos`:

```argos
function main() -> void {
    print("Hello, world!")
}
```

`print(...)` escreve os argumentos em sequência e termina a linha. Ele é a
operação de saída básica da linguagem: combina argumentos como `printf`, mas
adiciona a quebra de linha como `println`.

O próximo exemplo mostra variáveis, cálculo e saída:

```argos
function main() -> void {
    var a: int = 8
    var b: int = 9
    var mut c: int

    c = a + b
    print("O resultado é: ", c)
}
```

Quando `main` tem retorno `int`, use `return` para entregar um valor à função:

```argos
function sum(_ left: int, _ right: int) -> int {
    return left + right
}
```

## Rodar um programa

```bash
argosc run hello.argos
```

## Compilar um programa nativo

```bash
argosc compile hello.argos -o hello
./hello
echo $?
```

Programas com `main() -> void` terminam com status `0`; programas que retornam
`int` entregam esse valor como status de processo. A compilação nativa atual é
destinada a x86-64/Linux.

## Interpretar sem arquivo: REPL

```bash
argosc repl
```

```text
>>> 6 * 7
42
>>> print("Olá, Argos!")
Olá, Argos!
>>> :quit
```

O REPL executa expressões e instruções comuns, como `var`, atribuições, `if`,
`while` e `print(...)`, sem que seja necessário escrever `main`. Cada entrada
passa por lexer, parser, AST e análise semântica antes da interpretação.
Variáveis tipadas e inicializadas, bem como declarações de função, ficam
disponíveis para as entradas seguintes na mesma sessão. `:reset` limpa a
sessão inteira.

Para uma build com testes, habilite `-DARGOS_BUILD_TESTS=ON`. Na primeira vez,
o CMake pode precisar obter Catch2. Se o acesso à rede não estiver disponível,
use a configuração sem testes acima.

## Comandos de uso diário

```bash
# Valida um programa sem executá-lo
./build/bin/argosc check examples/control_flow.argos

# Executa na VM, sem criar binário nativo
./build/bin/argosc run examples/control_flow.argos

# Compila para x86-64/Linux
./build/bin/argosc compile examples/control_flow.argos -o control-flow
./control-flow

# REPL
./build/bin/argosc repl
```

`run` imprime o valor retornado por `main` (em uma função `void`, o valor é
`0`). O binário nativo devolve o retorno `int` como status do processo;
sistemas Unix preservam apenas `0..255`.

### Códigos de saída do CLI

| Código | Situação |
| --- | --- |
| `0` | comando concluído sem diagnóstico de erro |
| `64` | uso inválido do CLI |
| `65` | erro léxico, sintático, semântico ou de lowering |
| `66` | arquivo-fonte não pôde ser aberto |
| `70` | erro durante execução da VM |

## Estrutura de programa

Uma entrada executável precisa de `function main()` sem parâmetros e pode
retornar `void` ou `int`. Use `void` quando o programa só produz saída; use
`int` quando precisar devolver um código calculado:

```argos
function main() -> int {
    return 42
}
```

`function` é a única grafia aceita para declarar funções. `func` não é uma
palavra-chave.

Uma unidade de fonte contém declarações de função, declarações de variável e
instruções permitidas pelo frontend. Para gerar um executável, `main` deve
existir uma única vez e não receber parâmetros.

## Pontuação, blocos e terminadores

Blocos usam chaves. O lexer pode inserir terminadores em quebras de linha
elegíveis, por isso estes dois estilos são aceitos pelo subconjunto atual:

```argos
var value: int = 1
value = value + 1
```

```argos
var value: int = 1;
value = value + 1;
```

Quando houver dúvida durante a evolução da gramática, prefira `;`, sobretudo
após uma declaração sem inicializador. Ele torna a intenção inequívoca e reduz
diagnósticos em cascata.

## Tipos e valores

Os nomes canônicos são minúsculos: `int`, `bool`, `byte`, `char`, `string`,
`void`, além de `short`, `long`, `uint`, `ulong`, `float`, `double`, `usize` e
`isize`.

O subconjunto executável usa `int`, `bool`, `float`, `double`, `string`,
classes concretas, funções como valores e seus opcionais. Literais básicos:

```argos
var answer: int = 42
var enabled: bool = true
var disabled: bool = false
```

O tipo opcional usa `?`; `nil` representa ausência explícita. Um tipo
não-opcional **nunca** pode conter `nil`: a tentativa é erro semântico
`ARGOS-S0101` em declarações ou `ARGOS-S0331` em reatribuições, detectado em `check`.

```argos
var nome: string? = nil          // válido: string? admite nil
var outro: string = nil          // ARGOS-S0101: tipo incompatível
```

`null` ainda é aceito como apelido temporário de `nil` e será removido em
versão futura. Prefira sempre `nil`.

### Opcionais e arrays opcionais — implementação de 28/10/2026

Todos os operadores abaixo têm parser, semântica, IR, VM e backend x86-64/Linux.
O tipo-base precisa pertencer ao subconjunto executável.

| Operador | Regra | Resultado |
| --- | --- | --- |
| `??` | `T?` à esquerda, fallback compatível com `T`, avaliado apenas na ausência | `T` |
| `?.` | Base com uma camada opcional; omite membro/getter/chamada e argumentos na ausência | Tipo do membro/retorno tornado opcional, conservando opcionalidade existente |
| `?[` | Array `[T]?`; ausência omite índice; presença usa indexação normal | Elemento tornado opcional, conservando opcionalidade existente |
| `?=` | Fonte opcional; escreve uma camada desembrulhada somente na presença | Valor final do alvo |
| `?==` | Ao menos um opcional/nil; tipos-base compatíveis; aceita `T?`/`T` | `bool` |
| `??=` | Alvo opcional; avalia e escreve fonte somente quando alvo ausente | Valor final do alvo |
| `!!` | Remove uma camada; ausência falha com status 70 | Tipo desembrulhado |
| `!!=` | Fonte opcional; unwrap antes da escrita; ausência falha com status 70 | Valor final do alvo |

`null_type` é interno; inferência apenas de `nil` é rejeitada. `void?` e
`never?` são inválidos. `T` nunca recebe `nil` e `T?` nunca vira `T`
implicitamente. Não há conversão de opcional para `bool`, aritmética opcional,
smart cast, ternário ou propagação por `expressão?`. `!` é negação, `!!` é
pós-fixado. Opcionais têm até 4095 camadas.

`T??` mantém ausência externa distinta de presença com ausência interna;
`!!` e `??` removem uma camada. Cada conversão implícita envolve no máximo
uma camada. `?.`/`?[` conservam a opcionalidade de resultados já opcionais
para permitir navegação encadeada; bases com camadas adicionais exigem unwrap.
Chamadas seguras de métodos `void` são omitidas na ausência e não criam `void?`.

Arrays homogêneos usam `[T]`, literais `[1, 2]`, leitura/escrita `[]` e
indexação segura `?[` inspirada no Dart. `[T]?` torna o array opcional;
`[T?]` torna os elementos opcionais. Tipos de array são invariantes. Arrays têm
tamanho fixo e identidade por referência; escrita exige base mutável.
Arrays vazios ou somente com `nil` exigem contexto tipado. Índice exige `int`
não opcional, começa em zero e falha se negativo ou fora dos limites, mesmo
com `?[` quando a base estiver presente. Não há indexação de strings.

As atribuições opcionais exigem alvo mutável já inicializado, resolvido uma
vez antes da fonte. `??=` aceita fonte do tipo do alvo ou uma camada a menos.
Em propriedades, `??=` lê getter uma vez e só chama setter quando ausente;
`?=` lê getter na ausência para produzir o valor final e chama setter na
presença; `!!=` só chama setter após unwrap. Escritas por `?.`/`?[` são
rejeitadas. Falhas de unwrap e limites informam função, linha e coluna.

Igualdade compara camadas recursivamente, strings por conteúdo e
objetos/arrays por identidade. Funções não têm igualdade de valores, mas
funções opcionais podem ser comparadas com `nil`. `==`/`!=` exigem profundidades
iguais, além das comparações com `nil`; `?==` também admite uma camada de
diferença, envolvendo o operando menos opcional.

A precedência segue pós-fixados, prefixos, aritmética, relações, igualdade,
`&&`, `||`, `??` e atribuições. Coalescência e atribuições associam à direita.
Escritas em ramos puláveis não estabelecem inicialização definida.
O REPL executa os operadores e preserva opcionais numéricos/bool, inclusive
aninhados. Persistência de strings, objetos, arrays e closures continua
indisponível, assim como arrays multidimensionais, crescimento, slices/views,
`[T; N]` e `for`.

A representação usa boxes na arena e zero somente para a ausência externa,
sem reservar valores do payload. A IR valida tipos de wrapping, unwrapping,
arrays, chamadas e retornos. O [capítulo 11](guide/11-null-safety-e-opcionais.md)
contém exemplos e a matriz completa de suporte.

### Conversões e limites numéricos

No subconjunto executável, o valor de `int` é tratado como inteiro de 32 bits
com sinal na VM e no backend x86-64. Divisão por zero gera erro de runtime na
VM; overflow e a política de promoção numérica ainda não são contrato estável.
Não use o status de processo para testar retornos maiores que 255.

## Variáveis e mutabilidade

`var` declara uma variável **imutável por padrão**. Use `var mut` para uma
variável reatribuível:

```argos
function main() -> int {
    var base: int = 10
    var mut total: int = base
    total = total + 5
    return total
}
```

`let` não faz parte da sintaxe nova. Uma leitura antes da inicialização é erro
semântico; o compilador aponta o identificador e pode apresentar uma sugestão
de inicialização.

Não há inferência de mutabilidade: uma variável declarada com `var` nunca se
torna reatribuível por ter recebido uma atribuição posterior. Declare `var mut`
na origem quando esse for o propósito.

## Funções

Funções têm tipo de retorno após `->`. No subconjunto executável, elas podem
receber até seis parâmetros dos tipos executáveis, incluindo opcionais e arrays:

```argos
function multiply(_ left: int, _ right: int) -> int {
    return left * right
}

function main() -> int {
    return multiply(6, 7)
}
```

O `_` permite chamada posicional. Argumentos nomeados também executam,
preservando a ordem dos efeitos.

Chamadas são resolvidas semanticamente por nome, quantidade de argumentos,
rótulos e tipo. A ABI nativa atual passa até seis argumentos inteiros nos
registradores convencionais do alvo x86-64/Linux; reais usam registradores SSE.
Opcionais, arrays, objetos e closures usam palavras de referência. Métodos,
construtores e closures reservam uma posição implícita, deixando cinco
argumentos explícitos.

## Expressões

Estão disponíveis no subconjunto executável:

```argos
var result: int = 6 * 7 + 1
var comparison: bool = result >= 42
var negative: int = -result
```

Operadores incluem aritmética (`+`, `-`, `*`, `/`, `%`), comparação (`==`,
`!=`, `<`, `<=`, `>`, `>=`), negação (`!`) e atribuição (`=`).

As precedências seguem o comportamento usual: chamadas e postfixados têm
prioridade alta; multiplicação/divisão vêm antes de soma/subtração; comparações
vêm depois; atribuição associa à direita. Use parênteses quando a intenção não
for óbvia:

```argos
var result: int = (base + offset) * scale
```

## Fluxo de controle

### if / else

```argos
if enabled {
    return 7
} else {
    return 0
}
```

Corpos curtos também são aceitos:

```argos
if enabled return 7 else return 0
```

### while, break e continue

```argos
var mut index: int = 0
var mut total: int = 0

while index < 5 {
    index = index + 1
    if index == 3 continue
    total = total + index
    if total > 10 break
}
```

`break` sai do laço; `continue` volta para a condição do `while`. Ambos são
erros semânticos fora de um laço.

`for`, `switch`, `when` e `whenever` **não** fazem parte da v0.0.1 executável.
Use `while` e condicionais até que esses recursos atravessem as mesmas fases.

## Diagnósticos

Cada erro possui categoria e código:

- `ARGOS-Lxxxx`: léxico;
- `ARGOS-Pxxxx`: parsing;
- `ARGOS-Sxxxx`: semântica;
- `ARGOS-Cxxxx`: lowering/compilação.

Os diagnósticos carregam intervalo de origem. O apresentador de diagnósticos
mostra a linha, a coluna e, quando disponível, uma seção `ajuda` com uma
correção sugerida. Veja `examples/error_uninitialized.argos` e
`examples/error_immutable.argos`.

Um diagnóstico bem formado deve conter uma causa primária e evitar mensagens
derivadas da mesma falha. Por exemplo, uma variável não inicializada é apontada
no uso, não na função inteira:

```text
arquivo.argos:4:12: error[ARGOS-S0301]: variável "answer" usada antes da inicialização
  return answer
         ^
  ajuda: inicialize-a antes do uso: var answer: int = 0
```

Para erros de parser, corrija primeiro o diagnóstico de menor linha/coluna;
mensagens seguintes podem ser consequência da estrutura incompleta.

## Exemplos

| Arquivo | Uso |
| --- | --- |
| `hello_return.argos` | programa mínimo que retorna 42 |
| `variables.argos` | `var` e `var mut` |
| `functions_parameters.argos` | parâmetros e chamadas |
| `control_flow.argos` | `if`, `while`, `break`, `continue` |
| `minimal_break.argos` | saída antecipada de laço |
| `minimal_null_safety.argos` | operadores opcionais, propriedades e closures — VM e nativo |
| `minimal_optional_arrays.argos` | arrays opcionais e chamadas seguras — VM e nativo |
| `protocol_frontend.argos` | protocolos — apenas `check` |
| `error_uninitialized.argos` | erro intencional |
| `error_immutable.argos` | erro intencional |

## Limites desta versão

Classes com herança, `data`, enums, interfaces completas, `for`, `switch`,
`when`, genéricos, módulos, exceções, async/await e concorrência ainda estão em
evolução.

**Opcionais e null-safety** executam na VM e no nativo, incluindo `?[` de
arrays opcionais inspirado no Dart. O REPL preserva opcionais numéricos/bool;
veja os limites no [capítulo 11](guide/11-null-safety-e-opcionais.md).

`byte`, `char` e os demais tipos inteiros estreitos ou sem sinal ainda não são
executáveis. `float` e `double` usam representação binary64.

Também não há biblioteca padrão estável, concatenação de strings, mapas ou conjuntos,
interoperabilidade C exposta ao usuário, geração de objeto própria, cross
compilation ou suporte nativo para outros alvos. Consulte
`docs/language-evolution.md` para decisões de desenho e ordem de evolução.


## Avançado: inspeção do compilador

Estes comandos são úteis para estudar a implementação, depurar gramática ou
reportar um erro; não são necessários para escrever e executar programas:

```bash
# sequência de tokens, lexemas e posições
argosc lex examples/control_flow.argos

# confirma que o parser construiu um programa
argosc parse examples/control_flow.argos

# mostra a árvore sintática abstrata
argosc ast examples/control_flow.argos
```

## Avançado: estado interno e arquitetura

Esta é uma versão de bootstrap: o frontend reconhece mais construções do que a
IR executável consegue baixar. Recursos podem ser executáveis, apenas de
frontend ou planejados. O compilador aceita somente `function` e nomes de tipos
primitivos em minúsculas. `null` ainda é aceito como apelido temporário de
`nil`.

```text
arquivo .argos
  -> SourceManager
  -> Lexer
  -> Parser descendente recursivo + Pratt parser de expressões
  -> AST
  -> SemanticAnalyzer
  -> IR executável
  -> VM (`run`) ou x86-64/Linux (`compile`)
```

O parser não é LR/LALR/SLR gerado. Declarações e instruções usam parser manual;
expressões usam Pratt parsing para precedência e associatividade. O frontend
guarda intervalos de origem para alimentar os diagnósticos.


