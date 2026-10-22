# REPL

Inicie o interpretador interativo:

```bash
argosc repl
```

O REPL é uma sessão da linguagem, não uma calculadora isolada. Cada entrada
passa pelo lexer, parser, AST e análise semântica antes de ser interpretada.

```text
>>> var mut count: int = 0
>>> count = count + 1
1
>>> count
1
>>> var fallback: string? = nil
>>> fallback ?? "Anonymous"
Anonymous
```

Use `:help` para exemplos, `:reset` para descartar variáveis e funções da
sessão e `:quit` (ou `:q`) para sair.

As setas ↑ e ↓ navegam pelo histórico da sessão; Tab completa comandos do
REPL, palavras-chave, variáveis e funções conhecidas. Esse histórico não é
gravado em arquivo: ele e todo o contexto são apagados ao sair do REPL.

Uma expressão como `count + 1` calcula e mostra o resultado, mas não altera
`count`. Para atualizar uma variável, use atribuição e declare-a com `mut`.
