# Compilar, executar e diagnosticar

## Comandos essenciais

```bash
# analisa sintaxe e tipos
argosc check examples/control_flow.argos

# executa o subconjunto suportado pela VM
argosc run examples/control_flow.argos

# gera binário x86-64/Linux
argosc compile examples/control_flow.argos -o control-flow
./control-flow
```

`run` usa a VM. `compile` usa o backend nativo x86-64/Linux. O REPL também
compila cada entrada para a mesma IR e executa pela mesma VM; todos compartilham
o subconjunto executável descrito em [mvp.md](../mvp.md).

## Diagnósticos

Os códigos identificam a fase que encontrou o problema:

| Prefixo | Fase |
| --- | --- |
| `ARGOS-L` | lexer |
| `ARGOS-P` | parser |
| `ARGOS-S` | análise semântica |
| `ARGOS-C` | lowering ou compilação |

Exemplo:

```text
arquivo.argos:4:12: error[ARGOS-S0301]: variável "answer" usada antes da inicialização
  return answer
         ^
  falando simples: a variável ainda não recebeu um valor.
  tente assim: inicialize-a antes do uso: var answer: int = 0
```

Corrija primeiro o erro de menor linha e coluna: diagnósticos posteriores podem
ser apenas consequência do primeiro problema.
