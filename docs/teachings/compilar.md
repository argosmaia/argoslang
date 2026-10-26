# Compilar ArgosLang

Este guia distingue **compilar o compilador** de **compilar um programa `.argos`**. O `argosc` já gera executáveis para um subconjunto inicial em x86-64/Linux. Os demais recursos continuam disponíveis somente para `check` ou ainda estão em desenvolvimento.

## 1. Compilar o `argosc`

Na raiz do repositório, use CMake 3.20 ou superior, um compilador C++23 e a biblioteca ICU (`uc`). Para gerar um programa nativo também é necessário `cc` com assembler/linker x86-64. Para construir os testes, o CMake precisa obter Catch2; essa etapa pode exigir acesso à rede na primeira configuração.

```bash
cmake -S . -B build
cmake --build build
```

O executável produzido é `build/bin/argosc`. Para validar a construção:

```bash
ctest --test-dir build --output-on-failure
./build/bin/argosc check examples/minimal_arithmetic.argos
```

Se não for possível obter Catch2, construa somente o compilador:

```bash
cmake -S . -B build-no-tests -DARGOS_BUILD_TESTS=OFF
cmake --build build-no-tests
./build-no-tests/bin/argosc check examples/minimal_arithmetic.argos
```

## 2. Compilar e rodar um `.argos`

Na raiz do projeto, use um programa dentro do subconjunto, como `minimal_literal.argos`:

```bash
./build/bin/argosc examples/minimal_literal.argos
./minimal_literal
echo $?
```

O primeiro comando gera `minimal_literal` no **diretório de trabalho atual**, não necessariamente ao lado do `.argos`. O segundo executa o binário; `echo $?` mostra `42`, que é o retorno de `main()` visto como status de processo. Um nome simples sem `./` não costuma ser encontrado pelo shell, a menos que o diretório atual esteja no `PATH`.

### Seu próprio arquivo

Para `arquivo.argos`, a forma curta pedida é:

```bash
./build/bin/argosc arquivo.argos
./arquivo
echo $?
```

O nome da saída é o nome do fonte sem a extensão `.argos`. O arquivo precisa conter uma entrada `function main() -> int`; por exemplo:

```argos
function main() -> int {
    var answer: int = 6 * 7
    return answer
}
```

Se `argosc` estiver instalado no `PATH`, use `argosc arquivo.argos` no lugar de `./build/bin/argosc arquivo.argos`. A forma explícita também funciona: `./build/bin/argosc compile arquivo.argos`.

### Nomear a saída

Use `-o` para escolher o caminho do binário:

```bash
./build/bin/argosc compile examples/minimal_arithmetic.argos -o /tmp/argos-arithmetic
/tmp/argos-arithmetic
echo $?
```

Nesse caso o status esperado é `14`. O linker escreve uma saída temporária e só instala o executável após sucesso; `-o` não pode apontar para o próprio arquivo-fonte. Em sistemas Unix, o status do processo preserva apenas os oito bits baixos do retorno de `main()`: não use `echo $?` para verificar resultados fora de `0..255`. `print` escreve na saída padrão. Rode `echo $?` **imediatamente** após o executável, antes de outro comando alterar o status do shell.

**Limites atuais:** backend nativo x86-64/Linux; funções com até seis parâmetros `int`/`bool`/`float`/`double`; variáveis locais `var` e `var mut`, inclusive declarações tipadas sem inicializador; inteiros, números reais, booleanos e strings literais em `print`; aritmética inteira e real, comparações, atribuição, `if`, `while`, `break`, `continue`, chamadas e `return`. `/` sempre produz `double`, mesmo para operandos `int`; `%` é inteiro e erros de módulo por zero ou `int.min % -1` terminam com status 70. `float` e `double` usam binary64 nesta versão. `main` pode retornar `int` ou `void`. O backend gera assembly a partir da IR tipada/validada e usa `cc` apenas para montar e ligar, não para transpilar ArgosLang para C.

`examples/null_safety.argos` **ainda não compila para executável**: não define `main()` e usa protocolos, strings, opcionais e variáveis de topo, que não têm lowering nesse backend. `argosc check examples/null_safety.argos` pode validar o frontend, mas `argosc examples/null_safety.argos` termina com diagnóstico `ARGOS-C0001`/`ARGOS-C0002`, sem criar binário. O exemplo `minimal_optional.argos` também passa em `check`, porém ainda não compila para máquina.

Se a compilação falhar, leia o código e a posição do erro antes de tentar executar a saída. Um erro `ARGOS-C0001` indica uma construção válida para o frontend, mas ainda ausente do backend; `ARGOS-C0002` indica falta de `main()` compilável. Um `check` bem-sucedido não elimina esses erros de compilação. Também não confunda um executável antigo que já exista no disco com uma saída produzida por uma tentativa que falhou.

## 3. Verificar um `.argos` sem gerar executável

Os comandos do frontend são `lex`, `parse`, `ast` e `check`:

```bash
./build/bin/argosc lex examples/minimal_arithmetic.argos
./build/bin/argosc parse examples/minimal_arithmetic.argos
./build/bin/argosc ast examples/minimal_arithmetic.argos
./build/bin/argosc check examples/minimal_arithmetic.argos
```

`check` valida léxico, sintaxe e semântica. Se não houver erros, ele termina com código 0; **não** executa `main()` nem produz código-objeto. Um erro no arquivo termina com código 65. `ctest` inclui testes de execução nativa dos exemplos suportados no host x86-64/Linux.

## 4. Ampliar a compilação

O caminho executável atual e a arquitetura-alvo estão descritos em
[architecture.md](../architecture.md):

```text
.argos -> lexer -> parser manual -> semântica -> IR tipada e validada
       -> VM (direto da IR) ou assembly x86-64 -> cc (montagem/link)
```

Um arquivo de bytecode independente ainda não existe, e passar em `check` não
significa que toda construção do frontend esteja disponível para execução.

Para rodar um programa pela VM quando esse backend estiver pronto, consulte [interpretar.md](interpretar.md). Para distribuição e alvos de hardware, consulte [executar-em-maquinas.md](executar-em-maquinas.md).

### Gerar assembly

Para salvar o assembly GNU/AT&T x86-64 em vez do executável:

```bash
./build/bin/argosc compile examples/minimal_arithmetic.argos -S -o /tmp/programa.s
cc /tmp/programa.s -o /tmp/programa
/tmp/programa
echo $? # 14
```

A forma curta `argosc -S arquivo.argos` também funciona e gera `arquivo.s`
no diretório atual. A opção `-S` usa o backend existente sobre a IR tipada e
validada, sem chamar `cc`. Montar e ligar o resultado exige uma toolchain
x86-64/Linux. O subconjunto suportado é o mesmo da compilação nativa.
