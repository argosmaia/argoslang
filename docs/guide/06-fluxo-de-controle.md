# Fluxo de controle

## Condicionais

```argos
if enabled {
    print("enabled")
} else {
    print("disabled")
}
```

Para uma única instrução, também é possível escrever:

```argos
if enabled return 7 else return 0
```

## Laços

```argos
var mut index: int = 0

while index < 5 {
    print(index)
    index = index + 1
}
```

`break` encerra o laço atual e `continue` pula para a próxima avaliação da
condição. Usá-los fora de `while` é erro semântico.

`for`, `switch`, `when` e `whenever` ainda não pertencem à versão executável
0.0.1.
