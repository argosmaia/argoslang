# O primeiro programa

Crie `hello.argos`:

```argos
function main() -> void {
    print("Hello, world!")
}
```

Execute pela VM:

```bash
argosc run hello.argos
```

`main` é o ponto de entrada. Ele não recebe parâmetros e retorna `void` quando
o programa só produz saída. `print(...)` recebe um ou mais valores, escreve-os
em sequência e termina a linha.

```argos
function main() -> void {
    var a: int = 8
    var b: int = 9
    var mut total: int = a + b
    print("O resultado é: ", total)
}
```

Use retorno `int` quando o valor final for importante para outra função ou para
o status do processo:

```argos
function sum(_ left: int, _ right: int) -> int {
    return left + right
}
```
