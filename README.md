# Steelbox

Gerenciador de senhas pessoal escrito em C++, com armazenamento 100% local.

> **Status: núcleo em desenvolvimento.** O núcleo possui CRUD, persistência criptografada, validação, migrações, `SecureString`, busca e geração segura de senhas. CLI, TUI e revisão de segurança para uso real ainda estão pendentes.

## Objetivo

Guardar credenciais (usuário, senha, URL, notas, tags) em um cofre cifrado que fica **somente no seu computador**: sem servidor, sem conta, sem sincronização em nuvem. Você é o único dono dos seus dados.

## Princípios

- **Local primeiro:** nenhum dado sai da máquina. Não há servidor remoto.
- **Criptografia auditada:** o projeto usará uma biblioteca consagrada em vez de implementar primitivas próprias.
- **Núcleo reutilizável:** a lógica do cofre fica em uma biblioteca, e as interfaces (CLI, TUI, extensão) são camadas finas por cima dela.
- **Simples de entender:** código pequeno e legível, pensado também como projeto de aprendizado.

## Roadmap

**Núcleo**

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

- [ ] Host nativo em C++ (Native Messaging) **somente leitura**, com comandos mínimos: `status`, `unlock`, `lock`, `search` e `get_credentials`. Não existe comando de escrita.
- [ ] Extensão companheira (Manifest V3) para preencher usuário e senha em páginas de login. Ela usa o host nativo e não guarda dados por conta própria.
  - Casa a credencial pelo domínio exato e apenas em HTTPS
  - Preenche somente por ação do usuário e nunca em iframes de terceiros
  - Criar, editar e excluir entradas continua sendo feito pela CLI e pela TUI

**Distribuição**

- [ ] Binários para Windows, Linux e macOS, para uso a partir de um pendrive (a extensão exige registrar o host em cada computador)

## Estrutura do projeto

```
include/steelbox/models/   Cabeçalhos públicos (PasswordEntry, Vault)
src/                       Implementação e ponto de entrada (main.cpp)
src/models/                Implementação dos modelos
CMakeLists.txt             Configuração de build
```

## Como compilar

Requisitos: um compilador com suporte a C++20 (MSVC, GCC ou Clang) e CMake 3.25 ou superior.

```sh
cmake -S . -B build
cmake --build build
```

O executável `steelbox` será gerado dentro de `build/`. Por enquanto ele roda apenas uma demonstração das operações do cofre em memória.

## Segurança

O projeto está em fase inicial e **não passou por nenhuma auditoria**. Até que a criptografia esteja implementada e testada, não use o Steelbox para armazenar senhas reais.

## Licença

Ainda não definida.
