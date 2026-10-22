# Expressões e operadores

Expressões produzem valores. Operadores aritméticos trabalham com números:

```argos
var value: int = 6 * 7 + 1
var average: double = (2.5 + 3.5) / 2.0
```

Comparações produzem `bool`:

```swift
var valid: bool = value >= 42
var same: bool = "a" == "a"
```

Operadores disponíveis incluem `+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`,
`<=`, `>`, `>=`, `!`, `&&`, `||`, `=` e `??`.

Multiplicação e divisão têm precedência maior que soma e subtração. Use
parênteses quando a intenção não for evidente:

```argos
var result: int = (base + offset) * scale
```

No REPL, uma expressão sem efeito é exibida automaticamente:

```swift
>>> true == true
true
>>> 2.5 * 4.0
10
```
