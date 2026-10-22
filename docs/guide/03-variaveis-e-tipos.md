# Variáveis e tipos

Uma variável pode receber o primeiro valor na declaração ou depois. O
compilador não deixa ler uma variável antes de ela receber um valor:

```argos
var answer: int = 42
var name: string = "Argos"
var enabled: bool = true
var firstValue: int
firstValue = 42
var mut total: int
total = 42
```

O primeiro valor pode ser atribuído uma vez mesmo sem `mut`. Depois disso, a
variável fica imutável; use `var mut` quando precisar alterá-la novamente.

`var` é imutável. Para reatribuir, declare `var mut`:

```argos
var mut count: int = 0
count = count + 1
```

Isto é inválido:

```argos
var count: int = 0
count = count + 1 // erro: variável imutável
```

Os nomes canônicos de tipos são minúsculos: `bool`, `byte`, `char`, `short`,
`int`, `uint`, `long`, `ulong`, `float`, `double`, `string` e `void`. No
subconjunto executável atual, `float` também usa representação binary64.
Valores inteiros podem ser convertidos para `float` ou `double`, e valores
`float` e `double` podem ser usados entre esses dois tipos. Por isso, por
exemplo, `var value: float = 2` e `var value: float = 2.0` são válidos no MVP.

## Opcionais

`T?` representa um valor que pode estar ausente. A ausência explícita é `nil`:

```argos
var nickname: string? = nil
var display: string = nickname ?? "Anonymous"
```

O REPL interpreta esse exemplo. A rota de VM e compilação nativa ainda possui
um subconjunto menor de tipos; consulte a página de ferramentas.
