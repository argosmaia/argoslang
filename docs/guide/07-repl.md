# REPL

Inicie o shell interativo:

```bash
argosc repl
```

Cada entrada passa pelo mesmo lexer, parser, análise semântica, IR tipada e VM
usados por `argosc run`. O REPL mantém variáveis locais `int`/`bool` e funções
entre entradas; não usa um interpretador AST separado. Literais `string` podem
ser impressos, mas variáveis `string` ainda não são guardadas entre entradas.

```text
>>> var mut count: int = 0
>>> count = count + 1
1
>>> count
1
>>> function twice(_ value: int) -> int { return value * 2; }
>>> twice(count)
4
>>> var flag: bool = true
>>> print(flag)
true
```

Use `:help` para exemplos, `:reset` para descartar variáveis e funções da
sessão e `:quit` (ou `:q`) para sair.

As setas ↑ e ↓ navegam pelo histórico da sessão; Tab completa comandos do
REPL, palavras-chave, variáveis e funções conhecidas. Esse histórico não é
gravado em arquivo: ele e todo o contexto são apagados ao sair do REPL.

Uma expressão como `count + 1` calcula e mostra o resultado, mas não altera
`count`. Para atualizá-la, declare-a com `var mut` e use uma atribuição.
O REPL executa o mesmo subconjunto descrito em [mvp.md](../mvp.md); por
exemplo, opcionais e valores `string` armazenados ainda não são executáveis.
