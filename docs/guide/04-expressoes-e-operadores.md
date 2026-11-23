# Expressões e operadores

Expressões produzem valores. Operadores aritméticos trabalham com números:

```argos
let value: int = 6 * 7 + 1
let average: double = (2.5 + 3.5) / 2.0
let half: double = 1 / 2
```

Comparações produzem `bool`:

```argos
let valid: bool = value >= 42
let same: bool = "a" == "a"
```

## Contas e comparações

`+` soma, `-` subtrai, `*` multiplica, `/` divide e `%` encontra o resto de uma
divisão inteira. `==` pergunta se dois valores são iguais; `!=` pergunta se são
diferentes. `<`, `<=`, `>` e `>=` comparam números.

`/` sempre produz um valor real (`double`), mesmo quando os dois lados são
inteiros. Multiplicação e divisão têm precedência maior que soma e subtração. Use
parênteses quando a intenção não for evidente:

```argos
let base: int = 3
let offset: int = 2
let scale: int = 4
let result: int = (base + offset) * scale
```

## Verdadeiro e falso

`true` e `false` são valores do tipo `bool`. `!` inverte um booleano. `&&`
significa “e”; `||` significa “ou”. Eles param cedo quando já sabem a resposta:
em `false && ...`, o segundo lado não roda; em `true || ...`, também não.

```argos
let hasTicket: bool = true
if hasTicket {
    print("Pode entrar")
} else {
    print("Precisa de ingresso")
}
```

O frontend também entende `??`, que escolhe um valor quando um opcional está
vazio, mas opcionais ainda não fazem parte da execução completa do MVP:

```argos
let nickname: string? = nil
let displayName: string = nickname ?? "Visitante"
```

Outros operadores de opcionais (`?.`, `?=`, `?==` e `!!`) também não fazem
parte da execução completa do MVP. Consulte [MVP](../geral.md#mvp) antes de usá-los
em um programa que precisa rodar.

No REPL, uma expressão sem efeito é exibida automaticamente:

```text
>>> true == true
true
>>> 1 / 2
0.5
```
