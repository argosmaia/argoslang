# Variáveis e tipos

Uma variável pode receber o primeiro valor na declaração ou depois. O
compilador não deixa ler uma variável antes de ela receber um valor:

```argos
let answer: int = 42
let name: string = "Argos"
let enabled: bool = true
let firstValue: int
firstValue = 42
let mut total: int
total = 42
```

O primeiro valor pode ser atribuído uma vez mesmo sem `mut`. Depois disso, a
variável fica imutável; use `let mut` quando precisar alterá-la novamente.

`let` é imutável. Para reatribuir, declare `let mut`:

```argos
let mut count: int = 0
count = count + 1
```

Isto é inválido:

```argos
let count: int = 0
count = count + 1 // erro: variável imutável
```

Os nomes canônicos de tipos são minúsculos: `bool`, `byte`, `char`, `short`,
`int`, `uint`, `long`, `ulong`, `float`, `double`, `string` e `void`. No
subconjunto executável atual, `string` pode ser guardada em variáveis, passada
para funções, devolvida e impressa. `float` tem precisão binary32; a IR armazena o resultado arredondado em uma palavra binary64.
Valores inteiros podem ser convertidos para `float` ou `double`, e valores
`float` e `double` podem ser usados entre esses dois tipos. Por isso, por
exemplo, `let value: float = 2` e `let value: float = 2.0` são válidos no MVP.

## Números

Use `int` para números inteiros, como `7` ou `-12`. O lexer reconhece
inteiros em outras bases: `0b1010` é binário, `0o12` é octal e `0xA` é
hexadecimal. Também reconhece `_` para separar grupos, como em `1_000`.
Essas quatro bases e os separadores `_` funcionam com `run` e `compile`,
respeitando o intervalo do tipo executável.

Use `float` ou `double` para números com parte decimal ou expoente. Estas formas
decimais fazem parte do MVP executável:

```argos
let price: float = 2.5
let distance: double = 1.2e3
let half: double = 1 / 2
```

`float` arredonda para binary32; `double` usa binary64. A divisão
`/` sempre dá um resultado real: `1 / 2` vale `0.5`, não `0`. O operador `%`
calcula o resto da divisão e só aceita inteiros.

`byte` (0..255), `sbyte` (-128..127), `short` (-32768..32767) e `ushort`
(0..65535) também executam e verificam o intervalo em runtime. `int` usa 32 bits
e overflow modular, inclusive na negação do menor valor. `uint`, `long`, `ulong`,
`usize` e `isize` ainda não possuem lowering executável completo.

Strings são comparadas pelo conteúdo com `==` e `!=`. Ainda não existe
concatenação com `+`.

## Coleções

Arrays homogêneos já executam em arquivos:

```argos
let mut values: [int] = [1, 2, 3]
values[0] = 4
let optional: [int]? = values
print(optional?[0] ?? 0)
```

O tamanho é fixado na criação. `?[` trata ausência do array, sem ocultar
índices fora dos limites. Mapas, conjuntos, crescimento e `for` permanecem
pendentes. Veja [opcionais e arrays](11-null-safety-e-opcionais.md).

## Opcionais

`T?` admite presença de `T` ou ausência (`nil`); `T` não admite ausência.
Os operadores `??`, `?.`, `?[`, `?=`, `?==`, `??=`, `!!` e `!!=` executam
na VM e no nativo. O REPL preserva opcionais numéricos e booleanos.
O [capítulo 11](11-null-safety-e-opcionais.md) explica regras e limitações.

`var` foi removido: use `let` ou `let mut`. Arrays podem ser aninhados,
por exemplo `let matrix: [[int]] = [[1, 2], []]`.
