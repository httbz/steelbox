# Steelbox

Gerenciador de senhas pessoal escrito em C++, com armazenamento 100% local.

> **Status: base em desenvolvimento.** A base possui CRUD, persistência criptografada, validação, migrações, `SecureString`, busca e geração segura de senhas. CLI, TUI e revisão de segurança para uso real ainda estão pendentes.

## Objetivo

Guardar credenciais (usuário, senha, URL, notas, tags) em um cofre cifrado que fica **somente no seu computador**: sem servidor, sem conta, sem sincronização em nuvem.

## Princípios

- **Local:** nenhum dado sai da máquina. Não tem servidor remoto.
- **Criptografia auditada:** o projeto usará uma biblioteca consagrada em vez de implementar primitivas próprias.
- **Base reutilizável:** a lógica do cofre fica em uma biblioteca, e as interfaces (CLI, TUI, extensão) são camadas por cima dela.
- **Simples e legível:** código pequeno e legível, pensado como projeto de aprendizado.

## Roadmap

**Base**

- [x] Modelo de dados (`PasswordEntry`) e cofre em memória com CRUD (`Vault`)
- [x] Organização do repositório: `.gitignore`, remoção de artefatos de build e separação em biblioteca (`steelbox_core`) e executáveis
- [ ] Build portátil: linkagem estática (executável único, sem DLLs), cofre procurado ao lado do executável e nada gravado fora da pasta do programa
- [x] Criptografia: Argon2id para derivar a chave da senha mestra + cifragem autenticada (libsodium)
- [x] Formato de arquivo versionado, migração explícita, gravação atômica e backup
- [x] Busca no cofre, incluindo por domínio/URL normalizada
- [x] Gerador de senhas
- [x] Testes automatizados locais

**Interfaces**

- [ ] Interface de linha de comando (CLI): criar, listar, buscar, editar e remover entradas
- [ ] Interface de terminal interativa (TUI)

**Navegadores (Chrome e Firefox)**

- [ ] Host nativo em C++ (Native Messaging) **somente leitura**, com comandos mínimos: `status`, `unlock`, `lock`, `search` e `get_credentials`. Não tem comando de escrita.
- [ ] Extensão companheira (Manifest V3) para preencher usuário e senha em páginas de login. Ela usa o host nativo e não guarda dados por conta própria.
  - Casa a credencial pelo domínio exato e apenas em HTTPS
  - Preenche somente por ação do usuário e nunca em iframes de terceiros
  - Criar, editar e excluir entradas continua sendo feito pela CLI e pela TUI

**Distribuição**

- [ ] Binários para Windows, Linux e macOS, para uso a partir de um pendrive (para a extensão funcionar, tem que registrar o host em cada computador)

## Estrutura do projeto

```
include/steelbox/models/   Cabeçalhos públicos (PasswordEntry, Vault)
src/                       Implementação e ponto de entrada (main.cpp)
src/models/                Implementação dos modelos
CMakeLists.txt             Configuração de build
```

## Como compilar

Requisitos: um compilador com suporte a C++20 (MSVC, GCC ou Clang), CMake 3.25 ou superior e libsodium. No Windows, instale libsodium pelo vcpkg; em Linux/macOS, disponibilize-a via pkg-config.

```sh
cmake -S . -B build
cmake --build build
```

O executável `steelbox` será gerado dentro de `build/`.
No Windows, a compilação usa linkagem dinâmica e copia `libsodium.dll` para a mesma pasta de `steelbox.exe`.

## Testes, sanitizers e fuzzing

Execute os testes do núcleo com:

```sh
cmake --build build
ctest --test-dir build --output-on-failure
```

Em Linux, os sanitizers podem ser habilitados com Clang:

```sh
CC=clang CXX=clang++ cmake -S . -B build-sanitizers -DSTEELBOX_ENABLE_SANITIZERS=ON
cmake --build build-sanitizers
ctest --test-dir build-sanitizers --output-on-failure
```

O fuzzer do parser requer Clang com libFuzzer:

```sh
CC=clang CXX=clang++ cmake -S . -B build-fuzz -DSTEELBOX_BUILD_FUZZER=ON
cmake --build build-fuzz --target steelbox_parser_fuzz
./build-fuzz/steelbox_parser_fuzz -runs=1000 -max_len=1048576
```
