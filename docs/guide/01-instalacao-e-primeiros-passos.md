# Instalação e primeiros passos

## Dependências

No Ubuntu ou Pop!_OS:

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build g++ libicu-dev libreadline-dev
```

## Build

Na raiz do repositório:

```bash
cmake -S . -B build -G Ninja \
  -DARGOS_BUILD_TESTS=OFF \
  -DCMAKE_CXX_COMPILER=g++
cmake --build build
```

### Build com Clang

O sistema pode ter um `clang++` do Swift antes do Clang do Ubuntu no `PATH`.
Use o preset do projeto para selecionar explicitamente o Clang do sistema:

```bash
cmake --preset clang-system
cmake --build --preset clang-system
```

Ele gera o compilador em `./build-clang/bin/argosc` usando
`/usr/bin/clang++` (Clang 18), sem depender do `clang++` do Swift.

O executável gerado é `./build/bin/argosc`. A mensagem `ninja: no work to do`
significa apenas que o build já está atualizado.

### Erros de ligação com `std::cout`, `std::cerr` ou `__cxa_*`

Se o build usar `clang` no lugar de `clang++`, o driver pode compilar os
arquivos C++ sem ligar a biblioteca padrão. O CMake agora verifica isso
na configuração. A escolha do compilador fica salva no diretório de build;
mudar apenas o `PATH` não corrige o cache existente. O comando abaixo usa
as ferramentas do sistema durante a configuração, evitando wrappers do Swift.

Com CMake 3.24 ou superior, reconfigure:

```bash
env PATH=/usr/bin:/bin cmake --fresh -S . -B build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_AR=/usr/bin/ar -DCMAKE_RANLIB=/usr/bin/ranlib \
  -DARGOS_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Com CMake anterior, configure em um diretório novo. `g++` também é suportado;
no Ubuntu, use `-DCMAKE_CXX_COMPILER=/usr/bin/g++`.

## Usar no terminal

Durante a sessão atual:

```bash
export PATH="$PATH:$HOME/Projetos/argoslang/build/bin"
```

Para manter no Bash:

```bash
echo 'export PATH="$PATH:$HOME/Projetos/argoslang/build/bin"' >> ~/.bashrc
source ~/.bashrc
```

Depois disso, use `argosc`. Se o repositório estiver em outro diretório, ajuste
o caminho; alternativamente, continue usando `./build/bin/argosc`.
