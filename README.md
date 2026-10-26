# Steelbox

Gerenciador de senhas pessoal escrito em C++, com armazenamento 100% local.

> **Status: em desenvolvimento inicial.** O projeto ainda não é utilizável para guardar senhas reais. Hoje existe apenas o modelo de dados em memória; criptografia e persistência em disco são os próximos passos.

## Objetivo

Guardar credenciais (usuário, senha, URL, notas, tags) em um cofre cifrado que fica **somente no seu computador**: sem servidor, sem conta, sem sincronização em nuvem. Você é o único dono dos seus dados.

## Princípios

- **Local primeiro:** nenhum dado sai da máquina. Não há servidor remoto.
- **Criptografia auditada:** o projeto usará uma biblioteca consagrada em vez de implementar primitivas próprias.
- **Núcleo reutilizável:** a lógica do cofre fica em uma biblioteca, e as interfaces (CLI, TUI, extensão) são camadas finas por cima dela.
- **Simples de entender:** código pequeno e legível, pensado também como projeto de aprendizado.

## Roadmap

- [x] Modelo de dados (`PasswordEntry`) e cofre em memória com CRUD (`Vault`)
- [ ] Criptografia do cofre (derivação de chave a partir da senha mestra + cifragem autenticada)
- [ ] Formato de arquivo versionado e gravação atômica em disco
- [ ] Gerador de senhas
- [ ] Testes automatizados e integração contínua
- [ ] Interface de linha de comando (CLI)
- [ ] Interface de terminal interativa (TUI)
- [ ] Extensão para navegadores (Chrome e Firefox) via Native Messaging

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
