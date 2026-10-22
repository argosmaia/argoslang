# Funções

Funções usam `function`, parâmetros tipados e tipo de retorno após `->`:

```argos
function multiply(_ left: int, _ right: int) -> int {
    return left * right
}
```

O `_` torna o parâmetro posicional, portanto a chamada é:

```argos
var result: int = multiply(6, 7)
```

Uma função que não devolve valor usa `void`:

```argos
function greet(_ name: string) -> void {
    print("Hello, ", name)
}
```

No REPL, uma declaração de função permanece disponível até `:reset`:

```text
>>> function twice(_ value: int) -> int { return value * 2 }
>>> twice(21)
42
```
