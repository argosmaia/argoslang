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

O nome da saída é o nome do fonte sem a extensão `.argos`. O arquivo precisa conter uma entrada `func main() -> Int`; por exemplo:

```argos
func main() -> Int {
    let answer: Int = 6 * 7
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

Nesse caso o status esperado é `14`. O linker escreve uma saída temporária e só instala o executável após sucesso; `-o` não pode apontar para o próprio arquivo-fonte. Em sistemas Unix, o status do processo preserva apenas os oito bits baixos do retorno de `main()`: não use `echo $?` para verificar resultados fora de `0..255`. Ainda não há impressão automática do valor de retorno. Rode `echo $?` **imediatamente** após o executável, antes de outro comando alterar o status do shell.

**Limites atuais:** apenas x86-64/Linux, `func main() -> Int`, funções sem parâmetros, variáveis locais `Int`/`Bool`, literais decimais, aritmética, comparações, atribuição simples, `if`, `while`, chamadas sem argumentos e `return`. O backend emite assembly diretamente da IR e usa `cc` apenas para assembler/linker, não para transpilar ArgosLang para C. Divisão por zero e overflow ainda não têm uma política de runtime própria.

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

O caminho projetado em [architecture.md](../architecture.md) é:

```text
.argos -> lexer -> parser -> semântica -> IR Argos verificada
       -> backend nativo do alvo -> objeto/executável
```

Esse caminho está ligado ao CLI somente para a fatia executável descrita acima. O restante da IR, um verificador geral, formatos de objeto próprios, outras ABIs e backends de máquina ainda precisam ser implementados e testados. Passar em `check` não significa que a compilação nativa desse arquivo esteja disponível.

Para rodar um programa pela VM quando esse backend estiver pronto, consulte [interpretar.md](interpretar.md). Para distribuição e alvos de hardware, consulte [executar-em-maquinas.md](executar-em-maquinas.md).
