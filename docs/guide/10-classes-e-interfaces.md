# Classes, interfaces, herança e data

Herança simples, despacho polimórfico, sobrecarga, sobrescrita e propriedades
executam na VM e no nativo x86-64/Linux. Os modificadores são palavras-chave:
`virtual`, `abstract`, `override`, `overload` e `final`, sem `@`.

## Padrões de acesso

| Declaração | Acesso quando omitido |
| --- | --- |
| `class`, `abstract class`, `sealed class` | `public` |
| `data` | tipo `public`; componentes privados, imutáveis e com autorização `permited` |
| `function` fora de classe | `public` |
| `function` dentro de classe/data | método `private` |
| `interface` e seus requisitos | `public`, alterável explicitamente |

Acesso e modalidade são independentes: `abstract class` e `sealed class` são
públicas sem escrever `public`. `private`, `protected`, `internal` e `public`
explícitos substituem o padrão. `default class` restringe o tipo ao mesmo
`package` declarado; não equivale a omitir o modificador nem a `internal`.
As visibilidades explícitas são mutuamente exclusivas. Classes `private` e
`protected` só podem ser aninhadas. Uma classe aninhada pública continua sujeita
ao acesso da classe externa. Membros privados comuns respeitam o escopo nominal
aninhado; a autorização de componentes de `data` segue a regra exata descrita abaixo.

```argos
package br.argos.auth;
default class TokenValidator {}
class API {
    private class Hidden {}
    public class Result {}
}
```

Os arquivos do mesmo programa são fornecidos explicitamente:
`argosc check client.argos auth.argos`, `argosc run client.argos auth.argos` ou
`argosc compile client.argos auth.argos -o app`. Tipos podem ser referidos pelo
nome qualificado, como `br.argos.auth.API.Result`. Isso não introduz busca
automática de imports. A cláusula é `class Carro with Motor`, nunca `with class`.

## Campos, construtores e encapsulamento

```argos
class Counter {
    private var mut stored: int;
    public construct(initial: int) { stored = initial; }
    public function add(_ amount: int) -> int {
        stored += amount;
        return stored;
    }
}
function main() -> int {
    var counter = new Counter(initial: 1);
    var alias = counter;
    print(alias.add(2)); // 3; ambos referem-se ao mesmo objeto
    return 0;
}
```

Campos precisam de tipo. `var` permite uma única inicialização; `var mut`
permite alterações. O construtor deve inicializar cada campo obrigatório em
todos os caminhos. Inicializadores de campos não acessam a instância: use
`construct` para dependências entre campos. Durante o construtor, `this` não pode escapar, invocar métodos/acessores comuns
ou ser capturado por closures: uma base pode estar construindo um objeto
derivado cujos campos ainda não foram inicializados. Leitura/escrita direta
de campos segue a análise de inicialização.

`private` restringe o acesso à classe declarante, inclusive diante de subclasses.
`protected` permite acesso pela classe e suas subclasses. `internal` corresponde
ao módulo único atual; campos comuns e construtores explícitos sem acesso
conservam essa visibilidade nesta etapa. Métodos de classe são privados por
padrão. `public` permite acesso externo.
Uma implementação não pode reduzir a visibilidade do contrato herdado. Campos
herdados não podem ser ocultados por novos campos.

Construtores usam `construct(...)`, sem `function` e sem retorno, e são
selecionados por `new`. `init(...)` foi removido.

## Uma superclasse e contratos de interface

```argos
interface Readable { function read() -> int; }
class Source {
    protected var value: int;
    public construct(_ value: int) { this.value = value; }
    public virtual function read() -> int { return value; }
}
class Constant: Source by Readable {
    public construct(_ value: int) { super(value); }
    public override function read() -> int { return super.read() + 1; }
}
function main() -> int {
    var concrete = new Constant(41);
    var source: Source = concrete;
    var readable: Readable = concrete;
    print(source.read(), readable.read()); // 4242
    return 0;
}
```

Uma classe herda de uma única superclasse com `:` e implementa interfaces com
`by`: `class Funcionario: Pessoa by FormaPagamento { ... }`. A lista após `by`
pode conter várias interfaces. Usar `:` para implementar interface ou `by`
para herdar classe é erro. Interfaces podem estender interfaces com `:`, sem
estender classes. Interfaces e seus membros são públicos por padrão: basta escrever
`interface Readable`, sem `public interface`. Não armazenam campos nem contêm
implementações padrão nesta versão.

Métodos comuns são não virtuais. `virtual` e `abstract` permitem sobrescrita;
`override` é obrigatório ao substituir uma implementação herdada de classe.
A implementação de requisito de interface dispensa `override` e conserva ou
amplia o acesso declarado no contrato. Um requisito público exige implementação
explicitamente pública, pois métodos de classe são privados por padrão.
`final override` encerra a cadeia; classes `final` não admitem subclasses.
Uma classe concreta implementa todos os requisitos abstratos, com os mesmos
rótulos, parâmetros e retorno. Uma classe `abstract` não pode ser instanciada.

A assinatura é escolhida pelo tipo estático do receptor. A implementação de
uma chamada virtual é escolhida pelo tipo real do objeto, inclusive em métodos
da base, propriedades, closures e chamadas opcionais com `?.`.

## Sealed e permited

```argos
sealed class Pessoa permited Funcionario, Aluno {}
class Funcionario: Pessoa {}
class Aluno: Pessoa {}
```

`sealed` restringe as subclasses diretas às entidades da lista `permited`.
O tipo continua público por padrão. A lista explícita e não vazia é obrigatória.
A permissão não torna uma entidade descendente: a herança precisa ser declarada
com `:`. Cada nome deve existir, ser acessível e declarar a relação direta correspondente,
sem duplicação nem referência ao próprio tipo. `abstract sealed class` é válido.
Uma interface `sealed` também pode limitar seus implementadores diretos.
`sealed final` é inválido. A grafia da linguagem é **`permited`**; a forma
anterior é rejeitada com orientação para migração.

## Construção e acesso com super

`super(argumentos)` chama o construtor da superclasse e deve ser a primeira
instrução direta de `construct`, uma única vez. Os argumentos não podem observar
`this`. Se omitido, o compilador chama o construtor acessível sem argumentos da
base; se ele não existir, a declaração é rejeitada.

A ordem é: construção da base, componentes automáticos de `with` na ordem declarada,
inicializadores dos campos da subclasse e corpo
do construtor da subclasse. Construtores não são herdados. O construtor vazio
implícito segue as mesmas regras.

`super.metodo(...)` e `super.propriedade` selecionam a implementação da base,
sem redespachar para o override. O acesso conserva a visibilidade e não permite
invocar requisitos abstratos sem corpo. `super` não é um valor armazenável e
não pode ser capturado por lambda.

## Composição e delegação com with

```argos
class Motor {
    private var mut giros: int = 0;
    public function ligar() -> int { giros += 1; return giros; }
}
class GPS {
    public function localizar() -> string { return "Rio de Janeiro"; }
}
class Radio {
    public function tocar() -> string { return "musica"; }
}
class Carro with Motor, GPS, Radio {}
function main() -> int {
    var carro = new Carro();
    print(carro.ligar(), carro.localizar(), carro.tocar());
    return 0;
}
```

Cada `Carro` possui uma instância independente de cada componente. O compilador
gera armazenamento interno e encaminhamentos para métodos públicos de instância.
O estado e o receptor pertencem ao componente real. Métodos privados/protegidos,
construtores e acessores internos não são promovidos. `Carro` não se torna subtipo
de `Motor`, `GPS` ou `Radio`. Repetir o mesmo tipo na lista é erro.

A resolução usa nome e tipos dos parâmetros, preservando sobrecargas. Um método
explicitamente declarado na classe resolve a mesma assinatura. Sem ele, colisões
entre componentes ou entre um componente e um método herdado são erros. Dentro
da classe, `GPS.iniciar()` e `Radio.iniciar()` chamam suas respectivas instâncias;
não são chamadas estáticas. Para oferecer a resolução externamente, declare o
método como `public`, pois métodos próprios continuam privados por padrão.
Qualificadores completos distinguem componentes de pacotes diferentes.

`class Carro: Veiculo by Contrato with Motor, GPS` combina herança simples,
interfaces e composição. Um requisito abstrato herdado exige implementação
explícita na receptora; o encaminhamento automático não o satisfaz.

Um construtor acessível sem argumentos inicializa automaticamente o componente.
Para escolher outro construtor, use a atribuição já existente no corpo de
`construct`: `Motor = new Motor(4);`. Ela inicializa o armazenamento exclusivo,
uma única vez, com uma instância nova, e substitui a inicialização automática
somente naquele construtor. Todos os caminhos devem inicializar os componentes
obrigatórios. Essa atribuição é uma operação sem valor (`void`). Não permite
anexar uma instância já compartilhada. Todos os componentes precisam estar
prontos antes de qualquer chamada delegada; não podem ser usados nos argumentos
de `super(...)`. Os qualificadores de componente não podem escapar como valores
nem ser ocultados por variáveis locais ou parâmetros.

A composição mantém ownership exclusivo no modelo atual de arena. A VM valida
cada vínculo e libera os objetos em ordem inversa de alocação ao terminar a
execução; o nativo libera as alocações uma vez no encerramento, via `atexit`.
Não há destruição por escopo, `deinit`, borrowing, `with &T` ou `with shared T`.
Não foram introduzidos modos alternativos de memória nem mistura implícita deles.

## Sobrecarga e sobrescrita

```argos
function describe(_ n: int) -> int { return n; }
overload function describe(_ text: string) -> int { return 9; }
```

`overload` marca cada assinatura adicional de função, método ou construtor.
Os tipos e a quantidade dos parâmetros diferenciam as assinaturas; trocar
somente retorno ou rótulos é duplicação. Argumentos mantêm a avaliação na ordem
do código, mesmo quando os rótulos reorganizam os parâmetros.

Uma correspondência exata tem preferência sobre conversões; entre tipos
nominais compatíveis, o parâmetro mais específico tem preferência. A assinatura
vencedora precisa ser ao menos tão boa em cada argumento e melhor em algum
deles. A chamada precisa selecionar uma única assinatura compatível; `nil` entre duas assinaturas
opcionais pode ser ambíguo. Referências a nomes sobrecarregados exigem chamada
explícita ou uma função/lambda de assinatura única. `main` possui uma única
assinatura de entrada.

`override` substitui a mesma assinatura herdada. Uma assinatura diferente com
o mesmo nome é sobrecarga e exige `overload`. Membros não virtuais, privados ou
marcados `final` não podem ser sobrescritos.

## Propriedades com getter e setter

```argos
class Account {
    private var mut storedBalance: int = 0;
    public virtual var mut balance: int {
        getter() { return storedBalance; }
        setter() { storedBalance = value; }
    }
    public var doubled: int {
        getter() { return balance * 2; }
    }
}
```

`getter()` devolve o tipo da propriedade. `setter()` recebe `value` desse tipo
e retorna `void`; `setter(incoming)` permite outro nome. O setter exige `var mut`.
Sem setter, a propriedade é somente leitura. Propriedades são computadas, sem
armazenamento nem inicializador próprios.

`account.balance += 5` avalia o receptor uma vez, chama getter, avalia o valor e
chama setter. O resultado é o valor enviado ao setter, sem segunda leitura.
Os operadores opcionais conservam suas regras de curto-circuito.

Interfaces declaram `var name: string { getter(); }` ou
`var mut name: string { getter(); setter(); }`. A implementação deve preservar
todos os acessores exigidos. Propriedades `virtual`/`abstract` aceitam
`override`; getter e setter despacham pelo objeto real, inclusive por interface.
Ainda não há acessores automáticos nem visibilidade individual por acessor.

## Data: dados imutáveis com igualdade por conteúdo

```argos
data Point(x: int, label: string?);
data Box(point: Point);
function main() -> int {
    var a = new Box(point: new Point(x: 1, label: nil));
    var b = new Box(point: new Point(x: 1, label: nil));
    print(a == b); // true
    // a.point.x = 2; // erro: acesso a campo sealed não autorizado
    return 0;
}
```

`data` substitui o antigo agregado `protocol`, cuja sintaxe agora é rejeitada.
`interface` é o único mecanismo de contratos. `data Name(...)` gera campos
privados e imutáveis com autorização `permited` e o construtor público canônico com argumentos nomeados. Também são
aceitos `data class Name(...)` e `data class Name { ... }` com campos tipados e
construtor explícito quando necessário. O corpo pode declarar métodos e
implementar interfaces.

```argos
data Person(name: string) permited Funcionario, Aluno;
class Funcionario {
    public function nome(_ person: Person) -> string { return person.name; }
}
class Aluno {}
```

Campos de `data` só são acessíveis dentro do próprio tipo e das entidades
nominais explicitamente listadas em `permited`. Sem lista, apenas o próprio
tipo acessa os campos. A autorização é lexical e não é transferida às subclasses
de uma entidade autorizada. Ela permite acesso, sem permitir mutação. Igualdade
estrutural e o construtor canônico continuam disponíveis como operações do tipo.
A lista autoriza somente leitura dos componentes: não estabelece herança,
implementação de interface ou subtipagem. `data` é sempre não herdável. A permissão
não se transfere a tipos aninhados, subclasses, bibliotecas ou serializadores.
Tipos não autorizados podem construir, transportar, receber e retornar o valor,
respeitando a visibilidade do tipo e dos construtores.

Todo `data` é final e não estende classes. A imutabilidade é transitiva:
campos armazenam escalares, strings, opcionais desses tipos ou outros `data`.
`var mut`, propriedades computadas, arrays, closures, referências a classes
comuns e grafos recursivos de dados são rejeitados nesta etapa. Atribuição a
campos já inicializados é erro, inclusive dentro dos métodos do próprio `data`.

`==`, `!=` e `?==` com tipo estático `data` comparam os campos recursivamente.
Classes comuns conservam identidade. Converter um `data` para interface conserva
a referência de objeto; comparações pelo tipo interface usam identidade.
O compilador também gera `hashCode() -> int` e `toString() -> string` públicos;
`print(data)` usa a mesma representação textual. O hash de 32 bits é determinístico
por tipo e conteúdo, incluindo dados aninhados e opcionais; valores iguais do
mesmo tipo produzem hashes iguais. Os nomes desses métodos são reservados.
As implementações geradas também cumprem contratos de interface com as mesmas
assinaturas; chamadas pelo tipo interface preservam o despacho para o `data`.

Também é possível declarar componentes diretamente no corpo:

```argos
data Person permited PersonController {
    name: string;
    age: uint_8;
    email: string;
}
class PersonController {
    public function nome(_ person: Person) -> string { return person.name; }
}
```

Essa forma gera `construct(name: ..., age: ..., email: ...)` público.
`uint_8` é um inteiro sem sinal de **8 bits, um byte**, de 0 a 255, com validação
de faixa em atribuições e operações; não são oito bytes. Os nomes canônicos de
texto e booleano continuam `string` e `bool`. A representação tem a forma
`Person(name=Ada, age=25, email=ada@example.com)`.

Os valores usam a representação atual de objetos. Cópia inline, `copy`,
`componentN`, decomposição e serialização automática continuam pendentes.

## Diagnósticos e limites

| Código | Situação |
| --- | --- |
| `ARGOS-S0603`–`ARGOS-S0606` | ciclo, múltiplas superclasses, base final ou contrato incompatível |
| `ARGOS-S0608` | sobrescrita sem autorização, assinatura herdada ausente ou visibilidade reduzida |
| `ARGOS-S0609` | requisito abstrato sem implementação |
| `ARGOS-S0611` | membro ou construtor inacessível |
| `ARGOS-S0614` | instância ou campo parcialmente inicializado |
| `ARGOS-S0620` | assinatura adicional sem `overload` ou sobrecarga de `main` |
| `ARGOS-S0621` | chamada ambígua, sem assinatura compatível ou referência sobrecarregada |
| `ARGOS-S0622` | uso inválido de `super` |
| `ARGOS-S0623` | declaração de `data` viola imutabilidade |
| `ARGOS-S0624` | hierarquia ou campo sealed sem autorização em `permited`, ou lista inválida |
| `ARGOS-S0625` | uso incorreto de `:`/`by` |
| `ARGOS-S0626` | componente inválido, repetido, cíclico ou qualificador ambíguo |
| `ARGOS-S0627` | conflito de assinatura delegada |
| `ARGOS-S0628` | inicialização exclusiva ausente ou inválida |
| `ARGOS-S0629` | classe de topo privada/protegida |
| `ARGOS-S0630` | tipo inacessível por pacote ou contenedor |
| `ARGOS-S0631` | escape ou acesso inválido ao componente interno |
| `ARGOS-S0632` | nome reservado à geração de métodos de data |
| `ARGOS-S0330` | escrita em campo/binding imutável |

O [catálogo](12-diagnosticos.md) inclui explicações e correções. Exemplos completos:
`examples/minimal_polymorphism.argos`, `examples/minimal_data.argos` e
`examples/minimal_sealed_access.argos`, `examples/minimal_composition.argos`,
`examples/minimal_records.argos` e `examples/packages/`.

Objetos e closures sobrevivem ao retorno da função criadora, com armazenamento
liberado ao final da execução. Ownership/borrow, destruição por escopo e
`deinit` ainda não estão implementados. O REPL não preserva variáveis de objeto
ou closure. Também continuam pendentes métodos ligados como valores, `static`,
genéricos e captura mutável. O limite atual é de seis argumentos de
máquina, contando `this` ou o ambiente da closure.

## Validação desta implementação

A bateria atual contém 239 testes, aprovados com GCC e Clang em Linux x86-64.
Ela cobre parser, análise semântica, diagnósticos, validação de IR, execução na
VM, geração de assembly e execução nativa. Os cenários incluem composição
múltipla, conflitos de assinaturas, separação de estados, inicialização exclusiva,
pacotes em vários arquivos, permissões de `data` e liberação única de componentes.
A compilação local em `build/` também foi reconstruída.

Os métodos gerados de `data` foram executados por referência de interface nos
dois backends. A faixa de `uint_8` foi verificada dinamicamente: incrementar 255
encerra a execução com erro 70 antes de armazenar o resultado inválido.
