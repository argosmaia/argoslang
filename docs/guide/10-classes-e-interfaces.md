# Classes, interfaces e funções como valores

Classes concretas sem herança já executam pela VM e pelo nativo x86-64:

```argos
class Counter {
    private var mut value: int = 0;

    public function add(_ amount: int) -> int {
        value = value + amount;
        return this.value;
    }
}

function main() -> int {
    var counter = new Counter();
    var alias = counter;
    print(alias.add(2));
    print(counter.add(3)); // 5: ambos referem-se à mesma instância
    return 0;
}
```

Campos precisam de tipo. `var` conserva o valor após a primeira inicialização;
`var mut` permite alterações. Campos sem valor padrão precisam ser inicializados
em todos os caminhos de `construct`. Valores padrão não podem acessar a instância;
use `construct` para inicialização que dependa de outros campos. Construtores não
declaram retorno e só são chamados por `new`.

```argos
class Item {
    public var name: string;
    public construct(name: string) { this.name = name; }
}
```

`private` restringe ao tipo que declara o membro; `protected` permite acesso
pela classe e suas subclasses. `internal` é a visibilidade padrão e corresponde
ao módulo único atual. `public` permite acesso externo. Um binding imutável de
objeto pode referir-se a uma instância com campos mutáveis.

Interfaces e herança têm validação estática em `check`:

```argos
interface Readable { function read() -> int; }
abstract class Source : Readable {
    public abstract function read() -> int;
}
final class Constant : Source {
    public override function read() -> int { return 42; }
}
```

Esse segundo exemplo ainda é recusado por `run` e `compile`: despacho
polimórfico não é executável. Métodos são não virtuais por padrão; `virtual` e
`abstract` permitem sobrescrita com `override`. `final override` encerra a
cadeia. Interfaces declaram assinaturas de métodos e contratos de propriedades.
Não há sobrecarga, `static`, `sealed`, genéricos, `super` ou destrutores implementados.

[Funções e closures](05-funcoes.md) podem capturar uma referência imutável a
objeto e chamar seus métodos. Objetos e ambientes sobrevivem ao retorno da
função criadora, mas usam armazenamento com duração da execução; ainda não há
liberação por escopo. O REPL não preserva variáveis objeto ou closure.


## Propriedades com getter e setter

```argos
class Account {
    private var mut storedBalance: int = 0;

    public construct(initial: int) { this.storedBalance = initial; }

    public var mut balance: int {
        getter() { return this.storedBalance; }
        setter() { this.storedBalance = value; }
    }

    public var doubled: int {
        getter() { return this.balance * 2; }
    }
}

function main() -> int {
    var account = new Account(initial: 10);
    account.balance += 5;
    print(account.doubled); // 30
    return 0;
}
```

`getter()` devolve o tipo declarado da propriedade. `setter()` recebe `value`
desse mesmo tipo e retorna `void`; `setter(incoming)` usa `incoming` em vez de
`value`. O setter exige `var mut`. Sem setter, a propriedade é somente leitura,
mesmo quando declarada `var mut`.

A propriedade é computada: não possui armazenamento ou inicializador próprios.
Use um campo privado para guardar o valor. `account.balance += 5` avalia o
objeto uma vez, chama o getter, avalia `5` e chama o setter. O resultado da
atribuição é o valor enviado ao setter; não há uma segunda leitura implícita.

Interfaces descrevem contratos com `var name: string { getter(); }` ou
`var mut name: string { getter(); setter(); }`. A implementação deve oferecer
todos os acessores exigidos. Despacho executável por interfaces ainda não está
disponível. Em classes concretas, os acessores precisam de corpo; ainda não
existem acessores automáticos nem visibilidade diferente para cada acessor.

`init(...)` foi removido. Migre as declarações para `construct(...)`; não há
alias de compatibilidade. Os arquivos `examples/minimal_objects.argos` e
`examples/minimal_properties.argos` usam a sintaxe atual.
